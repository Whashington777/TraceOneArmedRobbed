#include "runtime.hpp"
#include "ue_api.hpp"
#include "override_value.hpp"
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
namespace trace::runtime {
namespace {
UEApi ue;std::mutex mutex;Config config;std::string state="Ожидание игрового потока";std::filesystem::path root;std::atomic<bool> restore{false},menu_visible{true};bool bound=false;std::atomic<bool> verified{false},initialized{false};std::string bridge_token;std::atomic<uint64_t> bridge_revision{0};std::filesystem::path runtime_file,bootstrap_root;DWORD game_thread=0;uint64_t ticks=0;
struct Saved{void* object;Identity id;std::wstring property;std::vector<uint8_t> original,last;bool touched=false;int element=-1;};std::vector<Saved> saved;
struct ScriptSaved{void* object;Identity id;void* data;std::vector<uint8_t> original;bool touched=false;};std::vector<ScriptSaved> scripts;
std::map<std::wstring,std::pair<void*,Identity>> function_cache;
struct BoolSaved{void* object;Identity id;std::wstring property;bool original,last,touched;};std::vector<BoolSaved> booleans;
struct CollisionSaved{void* object;Identity id;bool original,touched;};std::vector<CollisionSaved> collisions;
Identity card_player{};bool card_requested=false;
struct CursorSaved{void* object{};Identity id{};bool visible{},move{},look{};};CursorSaved cursor;
struct MovementSaved{void* object;Identity id;uint8_t mode;bool touched=false;};std::vector<MovementSaved> movement;
void Log(const std::string& msg){std::ofstream f(root/"runtime.log",std::ios::app);f<<GetTickCount64()<<" "<<msg<<'\n';}
void StatusSet(const std::string& msg){std::lock_guard<std::mutex> lock(mutex);if(state!=msg){state=msg;Log(msg);}}
bool Fingerprint(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);std::ifstream f(std::filesystem::path(path),std::ios::binary);if(!f)return false;BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD len=0,obj_len=0;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&obj_len),4,&len,0);std::vector<uint8_t> object(obj_len),buffer(1024*1024);bool ok=BCryptCreateHash(algorithm,&hash,object.data(),obj_len,nullptr,0,0)>=0;
 while(ok&&f){f.read(reinterpret_cast<char*>(buffer.data()),buffer.size());if(f.gcount())ok=BCryptHashData(hash,buffer.data(),DWORD(f.gcount()),0)>=0;}
 uint8_t digest[32]{};if(ok)ok=BCryptFinishHash(hash,digest,32,0)>=0;if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
 std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setw(2)<<std::setfill('0')<<int(b);
 return ok&&out.str()=="66ee255697a8167525c6e78b744525fd38aa643f2878e4c03a63330584248280";
}
void* Resolve(void* object,const wchar_t* property,size_t size,int element=-1){if(element<0)return ue.Ptr(object,property,int(size));auto a=static_cast<Array*>(ue.Ptr(object,property,16));if(!a||a->num<0||a->num>16||a->capacity<a->num||element>=a->num||!Readable(a->data,size*a->num,true))return nullptr;return static_cast<uint8_t*>(a->data)+size*element;}
Saved* Track(void* object,const wchar_t* property,size_t size,int element=-1){auto id=ue.Id(object);if(!id.cls)return nullptr;auto p=Resolve(object,property,size,element);if(!p)return nullptr;for(auto& s:saved)if(s.object==object&&s.property==property&&s.id==id&&s.element==element){s.touched=true;return &s;}
 Saved s{object,id,property,std::vector<uint8_t>(size),std::vector<uint8_t>(size),true};std::memcpy(s.original.data(),p,size);s.last=s.original;s.element=element;saved.push_back(std::move(s));return &saved.back();}
template<class T>void Value(void* o,const wchar_t* prop,T value,bool dynamic=false,bool multiplier=false,int element=-1){auto s=Track(o,prop,sizeof(T),element);if(!s)return;auto p=Resolve(o,prop,sizeof(T),element);T current{};std::memcpy(&current,p,sizeof(T));T base{},last{};std::memcpy(&base,s->original.data(),sizeof(T));std::memcpy(&last,s->last.data(),sizeof(T));OverrideValue<T> rule{base,last};value=rule.Apply(current,value,dynamic,multiplier);std::memcpy(s->original.data(),&rule.baseline,sizeof(T));std::memcpy(p,&value,sizeof(T));std::memcpy(s->last.data(),&value,sizeof(T));}
void Patch(const wchar_t* path){void* fn=nullptr;auto cached=function_cache.find(path);if(cached!=function_cache.end()&&ue.Id(cached->second.first)==cached->second.second)fn=cached->second.first;else{fn=ue.Find(path);if(fn)function_cache[path]={fn,ue.Id(fn)};}auto a=ue.Script(fn);if(!a)return;auto id=ue.Id(fn);for(auto& s:scripts)if(s.object==fn&&s.id==id&&s.data==a->data){s.touched=true;return;}auto p=static_cast<uint8_t*>(a->data);
  
 if(p[0]!=0x46&&p[0]!=0x64){Log("Rejected non-wrapper bytecode");return;}ScriptSaved s{fn,id,a->data,std::vector<uint8_t>(p,p+3),true};scripts.push_back(std::move(s));p[0]=0x04;p[1]=0x0b;p[2]=0x53;
}
void Boolean(void* o,const wchar_t* prop,bool value){auto id=ue.Id(o);if(!id.cls)return;bool old=false;if(!ue.BoolField(o,prop,old))return;auto it=std::find_if(booleans.begin(),booleans.end(),[&](auto& b){return b.object==o&&b.id==id&&b.property==prop;});if(it==booleans.end()){booleans.push_back({o,id,prop,old,value,true});it=booleans.end()-1;}else{if(old!=it->last)it->original=old;it->last=value;it->touched=true;}ue.BoolField(o,prop,value,true);}
void Noclip(void* player){auto id=ue.Id(player);auto it=std::find_if(collisions.begin(),collisions.end(),[&](auto& b){return b.object==player&&b.id==id;});if(it==collisions.end()){bool old=false;if(!ue.BoolCall(player,L"GetActorEnableCollision",old)||!ue.BoolArgument(player,L"SetActorEnableCollision",false))return;collisions.push_back({player,id,old,true});}else it->touched=true;}
void GiveCard(void* player){
 auto cls=ue.Find(L"/Game/BP/Hacking/Item_Keycard.Item_Keycard_C");auto statics=ue.Find(L"/Script/Engine.Default__GameplayStatics");if(!cls||!statics){Log("Keycard spawn: class not loaded");return;}
 float location[3]{};if(!ue.Call(player,L"K2_GetActorLocation",{{L"ReturnValue",location,12,true}})){Log("Keycard spawn: location ABI rejected");return;}
  
 alignas(16) float transform[12]={0,0,0,1,location[0]+80,location[1],location[2]+30,0,1,1,1,0};
 void* actor=nullptr;void* owner=player;uint8_t collision=1;
 if(!ue.Call(statics,L"BeginDeferredActorSpawnFromClass",{{L"WorldContextObject",&player,8},{L"ActorClass",&cls,8},{L"SpawnTransform",transform,48},{L"CollisionHandlingOverride",&collision,1},{L"Owner",&owner,8},{L"ReturnValue",&actor,8,true}})||!ue.Valid(actor)){Log("Keycard spawn: deferred ABI rejected");return;}
 auto number=ue.Ptr(actor,L"KeycardNumber",4);if(number){int zero=0;std::memcpy(number,&zero,4);}
 void* finished=nullptr;if(!ue.Call(statics,L"FinishSpawningActor",{{L"Actor",&actor,8},{L"SpawnTransform",transform,48},{L"ReturnValue",&finished,8,true}})){ue.Call(actor,L"K2_DestroyActor",{});Log("Keycard spawn: finish ABI rejected");return;}
 Log("Universal keycard spawned beside local player");
}
void RestoreUntouched(){
 for(auto it=booleans.begin();it!=booleans.end();){if(!it->touched){bool current=false;if(ue.Id(it->object)==it->id&&ue.BoolField(it->object,it->property.c_str(),current)&&current==it->last)ue.BoolField(it->object,it->property.c_str(),it->original,true);it=booleans.erase(it);}else ++it;}
 for(auto it=collisions.begin();it!=collisions.end();){if(!it->touched){if(ue.Id(it->object)==it->id)ue.BoolArgument(it->object,L"SetActorEnableCollision",it->original);it=collisions.erase(it);}else ++it;}

 for(auto it=saved.begin();it!=saved.end();){if(!it->touched){if(ue.Id(it->object)==it->id){auto p=Resolve(it->object,it->property.c_str(),it->original.size(),it->element);if(p&&std::memcmp(p,it->last.data(),it->last.size())==0)std::memcpy(p,it->original.data(),it->original.size());}it=saved.erase(it);}else ++it;}
 for(auto it=scripts.begin();it!=scripts.end();){if(!it->touched){if(ue.Id(it->object)==it->id){auto a=ue.Script(it->object);if(a&&a->data==it->data){auto p=static_cast<uint8_t*>(a->data);if(p[0]==0x04&&p[1]==0x0b&&p[2]==0x53)std::memcpy(p,it->original.data(),3);}}it=scripts.erase(it);}else ++it;}
 for(auto it=movement.begin();it!=movement.end();){if(!it->touched){if(ue.Id(it->object)==it->id)ue.SetMovementMode(it->object,it->mode);it=movement.erase(it);}else ++it;}
}
void CaptureInput(void* controller,bool visible){
 if(cursor.object&&(!visible||cursor.object!=controller||!(ue.Id(cursor.object)==cursor.id))){if(ue.Id(cursor.object)==cursor.id){bool old=cursor.visible;ue.BoolField(cursor.object,L"bShowMouseCursor",old,true);if(cursor.move)ue.BoolArgument(cursor.object,L"SetIgnoreMoveInput",false);if(cursor.look)ue.BoolArgument(cursor.object,L"SetIgnoreLookInput",false);}cursor={};}
 if(visible&&controller){if(!cursor.object){cursor.object=controller;cursor.id=ue.Id(controller);ue.BoolField(controller,L"bShowMouseCursor",cursor.visible);cursor.move=ue.BoolArgument(controller,L"SetIgnoreMoveInput",true);cursor.look=ue.BoolArgument(controller,L"SetIgnoreLookInput",true);}bool show=true;ue.BoolField(controller,L"bShowMouseCursor",show,true);}
}
void Fly(void* player,void* move,float speed){auto id=ue.Id(move);if(!id.cls)return;auto p=ue.Ptr(move,L"MovementMode",1);if(!p)return;auto it=std::find_if(movement.begin(),movement.end(),[&](auto& s){return s.object==move&&s.id==id;});if(it==movement.end()){MovementSaved s{move,id,*static_cast<uint8_t*>(p),true};if(!ue.SetMovementMode(move,5))return;movement.push_back(s);}else it->touched=true;
 Value<float>(move,L"MaxFlySpeed",speed,true,true);float z=0;if(GetAsyncKeyState(VK_SPACE)&0x8000)z+=1;if(GetAsyncKeyState(VK_LCONTROL)&0x8000)z-=1;if(z)ue.AddVerticalInput(player,z);
}
}
std::vector<std::string> Supported(){return {
"stealth.no_phone","stealth.stop_phone","stealth.guard_ignore","stealth.camera_ignore","stealth.phone_time","stealth.spot_multiplier","stealth.emp_extended","stealth.emp_radius",
"tools.universal_card","stealth.quiet_glass","movement.noclip","movement.fly","movement.fly_speed","movement.speed","movement.multiplier","movement.jump",
"player.god","player.no_fall","player.regen","player.fast_revive","player.max_health",
"weapon.no_recoil","weapon.no_spread","weapon.infinite_mag","weapon.infinite_reserve","weapon.fast_reload","weapon.reload","weapon.damage","weapon.grenade_time",
"tools.fast_lock","tools.lock_speed","tools.no_hack_cooldown","tools.fast_powerbox","tools.powerbox_time",
"heist.custom_take","heist.minimum_take"};}
void InitializePath(void* module){wchar_t path[32768]{};GetModuleFileNameW(static_cast<HMODULE>(module),path,32768);runtime_file=std::filesystem::path(path);root=runtime_file.parent_path();wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);bootstrap_root=std::filesystem::path(executable).parent_path()/"Mods"/"TraceOAR";verified=Fingerprint();initialized=true;Log(verified?"Build hash verified":"Build hash mismatch: gameplay disabled");}
void PollAttach(){
 if(!initialized||!verified)return;static uint64_t next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;
 std::ifstream request(bootstrap_root/"attach.request");std::string token;std::getline(request,token);if(token.empty()||token.size()>256)return;
 static std::string previous;if(token==previous&&std::filesystem::exists(bootstrap_root/"attach.response"))return;
 auto wide=runtime_file.wstring();int length=WideCharToMultiByte(CP_UTF8,0,wide.data(),int(wide.size()),nullptr,0,nullptr,nullptr);std::string path(length,'\0');WideCharToMultiByte(CP_UTF8,0,wide.data(),int(wide.size()),path.data(),length,nullptr,nullptr);
 auto temporary=bootstrap_root/"attach.response.new";{std::ofstream response(temporary,std::ios::binary);if(!response)return;response<<token<<'\n'<<path<<'\n';if(!response)return;}
 if(MoveFileExW(temporary.c_str(),(bootstrap_root/"attach.response").c_str(),MOVEFILE_REPLACE_EXISTING)){previous=token;{std::lock_guard<std::mutex> lock(mutex);bridge_token=token;}bridge_revision.fetch_add(1);Log("Bootstrap handshake published");}
}
void SetConfig(const Config& c){std::lock_guard<std::mutex> lock(mutex);if(config==c)return;config=c;
 static std::string previous_requests;auto enabled=[&](const char* id){auto it=c.find(id);return it!=c.end()&&it->second!=0;};std::string requested;
 if(enabled("tools.fast_lock"))requested+="Lock_pick_C\n";
 if(enabled("tools.universal_card"))requested+="Item_Keycard_C\n";
 if(enabled("stealth.quiet_glass"))requested+="BreakableGlass_C\nDisplayBreakableGlass_C\n";
 if(enabled("tools.no_hack_cooldown")||enabled("tools.universal_card"))requested+="BP_HackingPoint_C\n";
 if(enabled("tools.fast_powerbox"))requested+="BP_Powerbox_C\n";
 if(enabled("stealth.emp_extended"))requested+="Tool_Grenade_EMP_C\n";
 auto grenade=c.find("weapon.grenade_time");if(grenade!=c.end()&&grenade->second!=3)requested+="Tool_Grenade_base_C\n";
 if(enabled("heist.custom_take"))requested+="RobberTruck_C\n";
 if(enabled("stealth.stop_phone"))requested+="GuardPhone_C\n";
 if(requested!=previous_requests||!std::filesystem::exists(root/"needed.txt")){std::ofstream(root/"needed.txt")<<requested;previous_requests=requested;}
}
void RequestRestore(){restore=true;}
void SetMenuVisible(bool visible){menu_visible=visible;}
std::string Status(){std::lock_guard<std::mutex> lock(mutex);return state;}
void Tick(){
 LARGE_INTEGER start{},frequency{};QueryPerformanceCounter(&start);QueryPerformanceFrequency(&frequency);
 struct Timing {LARGE_INTEGER start,frequency;~Timing(){LARGE_INTEGER end{};QueryPerformanceCounter(&end);double ms=1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart;static uint64_t next=0;if(ms>5&&GetTickCount64()>next){next=GetTickCount64()+10000;Log("Slow native tick: "+std::to_string(ms)+" ms");}}} timing{start,frequency};
 if(!initialized)return;
 if(!verified){StatusSet("Сборка игры не совпадает: функции отключены");return;}
 if(!bound){bound=ue.Bind();if(!bound){StatusSet("Не найден API UE4SS 3.0.1");return;}}
 if(!game_thread)game_thread=GetCurrentThreadId();if(game_thread!=GetCurrentThreadId()){StatusSet("Вызов вне игрового потока отклонён");return;}
 for(auto& s:booleans)s.touched=false;for(auto& s:collisions)s.touched=false;for(auto& s:saved)s.touched=false;for(auto& s:scripts)s.touched=false;for(auto& s:movement)s.touched=false;
 Config c;{std::lock_guard<std::mutex> lock(mutex);c=config;}auto v=[&](const char* id,float fallback=0){auto it=c.find(id);return it==c.end()?fallback:it->second;};
 std::ifstream input(root/"objects.txt");std::vector<std::pair<std::string,void*>> objects;std::string kind;uintptr_t address;while(input>>kind>>std::hex>>address){if(objects.size()>=8192)break;auto o=reinterpret_cast<void*>(address);if(ue.Valid(o))objects.emplace_back(kind,o);}
 void* player=nullptr;for(auto& [type,o]:objects)if(type=="PlayerCharacter_C"){bool local=false,authority=false;if(ue.BoolCall(o,L"IsLocallyControlled",local)&&local&&ue.BoolCall(o,L"HasAuthority",authority)&&authority){player=o;break;}}
 static uint64_t ready_revision=0,next_ready=0;auto revision=bridge_revision.load();auto current_time=GetTickCount64();if(player&&(revision!=ready_revision||current_time>=next_ready)){std::string token;{std::lock_guard<std::mutex> lock(mutex);token=bridge_token;}if(!token.empty()){std::ofstream ready(bootstrap_root/"attach.ready");ready<<token;if(ready){if(ready_revision!=revision)Log("Gameplay bridge ready on local host Tick");ready_revision=revision;next_ready=current_time+1000;}}}
 CaptureInput(player?ue.Object(player,L"Controller"):nullptr,player&&menu_visible);
 if(restore.exchange(false)){c.clear();}  
 if(!player||c.empty()){RestoreUntouched();StatusSet(player?"Изменения восстановлены":"Ожидание локального игрока-хоста");return;}
 if(v("stealth.no_phone"))Patch(L"/Game/BP/NPC/NPC_Guard.NPC_Guard_C:SpawnPhone");
 if(v("stealth.camera_ignore"))Patch(L"/Game/BP/Camera/CameraBP.CameraBP_C:CameraCheck");
 if(v("stealth.guard_ignore")){Patch(L"/Game/BP/NPC/NPC_Guard.NPC_Guard_C:See pawn");Patch(L"/Game/BP/NPC/NPC_Guard.NPC_Guard_C:BndEvt__NPC_Guard_SpotPlayerComponent_K2Node_ComponentBoundEvent_0_PlayerSpotted__DelegateSignature");}
 if(v("stealth.guard_ignore")){
 Patch(L"/Game/BP/NPC/Civilian_NPC.Civilian_NPC_C:BndEvt__Civilian_NPC_SpotPlayerComponent_K2Node_ComponentBoundEvent_5_PlayerSpotted__DelegateSignature");
 Patch(L"/Game/BP/NPC/Civilian_NPC.Civilian_NPC_C:BndEvt__Civilian_NPC_PawnSensing_K2Node_ComponentBoundEvent_1_SeePawnDelegate__DelegateSignature");
 Patch(L"/Game/BP/NPC/Police/NPC_Police_base.NPC_Police_base_C:BndEvt__NPC_Police_PawnSensing_K2Node_ComponentBoundEvent_0_SeePawnDelegate__DelegateSignature");
 Patch(L"/Game/BP/NPC/Police/NPC_Police_base.NPC_Police_base_C:SetTarget");
 Patch(L"/Game/BP/NPC/Police/NPC_Police_base.NPC_Police_base_C:Shoot");
 }
 bool want_card=v("tools.universal_card")!=0;auto player_id=ue.Id(player);if(want_card&&(!card_requested||!(card_player==player_id)))GiveCard(player);card_requested=want_card;card_player=player_id;
 if(want_card){Patch(L"/Game/BP/Hacking/BP_HackingPoint.BP_HackingPoint_C:BndEvt__HackingPoint_AlertComponent_K2Node_ComponentBoundEvent_0_OnAlarmtriggered__DelegateSignature");Patch(L"/Game/BP/Hacking/Item_Keycard.Item_Keycard_C:AlarmTriggeredInterfaceCall");}
 if(v("stealth.stop_phone"))Patch(L"/Game/BP/Hacking/GuardPhone.GuardPhone_C:AlertGuards");
 if(v("player.god")){Patch(L"/Game/BP/Player/PlayerCharacter.PlayerCharacter_C:TakeDamage");Patch(L"/Game/BP/Player/PlayerCharacter.PlayerCharacter_C:TakeDamageInterface");}
 if(v("player.no_fall"))Patch(L"/Game/BP/Player/PlayerCharacter.PlayerCharacter_C:FallDamage");
 if(v("weapon.no_recoil"))Patch(L"/Game/BP/Player/PlayerCharacter.PlayerCharacter_C:Recoil");
 auto move=ue.Object(player,L"CharacterMovement");if(move){if(v("movement.speed"))Value<float>(move,L"MaxWalkSpeed",v("movement.multiplier",1),true,true);if(v("movement.jump",1)!=1)Value<float>(move,L"JumpZVelocity",v("movement.jump",1),true,true);if(v("movement.noclip"))Noclip(player);if(v("movement.fly")||v("movement.noclip"))Fly(player,move,v("movement.fly_speed",1));}
 if(v("player.max_health",100)!=100)Value<int>(player,L"MaxHealth",int(v("player.max_health")));
 if(v("player.fast_revive"))Value<float>(player,L"ReviveTime",.1f);
 if(v("player.regen"))Value<float>(player,L"AddedHealSpeed%",1000.f);
 if(v("weapon.no_spread"))Value<float>(player,L"AddedAccuracy",10000.f);
 if(v("stealth.phone_time"))Value<int>(player,L"AddedGuardPhoneTime",int(v("stealth.phone_time")));
 if(v("stealth.spot_multiplier",1)!=1){Value<float>(player,L"AddedCamSpotTime%",(v("stealth.spot_multiplier")-1)*100);Value<float>(player,L"AddedGuardSpotTime%",(v("stealth.spot_multiplier")-1)*100);}
 if(v("weapon.infinite_reserve")){auto a=static_cast<Array*>(ue.Ptr(player,L"ReserveAmmo",16));if(a&&a->num>0&&a->num<=16)for(int i=0;i<a->num;++i)Value<int>(player,L"ReserveAmmo",999,true,false,i);}
 auto gun=ue.Object(player,L"HoldingGun");if(gun){if(v("weapon.infinite_mag")){auto size=ue.Ptr(gun,L"MagSize",4);if(size){int n{};std::memcpy(&n,size,4);if(n>0&&n<=1000)Value<int>(gun,L"BulletsLeft",n,true);}}if(v("weapon.fast_reload"))Value<float>(gun,L"CalculatedReloadTime",.1f);auto defaults=ue.DefaultObject(gun);if(defaults){if(v("weapon.fast_reload"))Value<float>(defaults,L"Reload time",1.f/std::max(1.f,v("weapon.reload",5)),false,true);if(v("weapon.damage",1)!=1){auto s=Track(defaults,L"Damage",4);if(s){int base{};std::memcpy(&base,s->original.data(),4);Value<int>(defaults,L"Damage",int(std::lround(base*v("weapon.damage",1))));}}}}
 for(auto& [type,o]:objects){
  if((type=="BreakableGlass_C"||type=="DisplayBreakableGlass_C")&&v("stealth.quiet_glass"))Value<uintptr_t>(o,L"AlertComponent",0);
  if(type=="Item_Keycard_C"&&want_card)Value<int>(o,L"KeycardNumber",0);
  if(type=="BP_HackingPoint_C"&&want_card){Value<int>(o,L"KeycardNumber",0);Boolean(o,L"CanUseAfterAlarm?",true);Boolean(o,L"Disabled?",false);}
  if(type=="GuardPhone_C"&&v("stealth.stop_phone")){auto entry=Track(o,L"SecondsToAlert",4);if(entry){int baseline=1;std::memcpy(&baseline,entry->original.data(),4);Value<int>(o,L"SecondsToAlert",std::max(1,baseline));}}
  if(type=="Lock_pick_C"&&v("tools.fast_lock")){Value<float>(o,L"PickMultiplier",v("tools.lock_speed",1),false,true);Value<float>(o,L"PickMaxChange",1.f);}
  if(type=="BP_HackingPoint_C"&&v("tools.no_hack_cooldown"))Value<float>(o,L"HackCooldown",0.f);
  if(type=="BP_Powerbox_C"&&v("tools.fast_powerbox"))Value<float>(o,L"DestroyTime",v("tools.powerbox_time",60));
  if(type=="Tool_Grenade_EMP_C"&&v("stealth.emp_extended"))Value<float>(o,L"EMPRadius",v("stealth.emp_radius",8)*100);
  if(type=="Tool_Grenade_base_C"&&v("weapon.grenade_time",3)!=3)Value<float>(o,L"TriggerTime",v("weapon.grenade_time",3));
  if(type=="RobberTruck_C"&&v("heist.custom_take"))Value<int>(o,L"MinimumTake",int(v("heist.minimum_take",20000)));
 }
 RestoreUntouched();++ticks;StatusSet("Хост найден · активных изменений: "+std::to_string(saved.size()+scripts.size()+movement.size()));
 if(ticks%300==0)Log("Heartbeat: "+std::to_string(objects.size())+" objects; "+std::to_string(saved.size())+" properties; "+std::to_string(scripts.size())+" scripts");
}
}
extern "C" __declspec(dllexport) int TraceTick(void*){trace::runtime::Tick();return 0;}
