#include "menu.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
namespace trace {
namespace {
ImVec4 accent{250.f/255,136.f/255,93.f/255,1};
ImU32 Col(int r,int g,int b,int a=255){return IM_COL32(r,g,b,int(a*ImGui::GetStyle().Alpha));}
ImU32 Accent(){auto c=accent;c.w*=ImGui::GetStyle().Alpha;return ImGui::ColorConvertFloat4ToU32(c);}
const std::unordered_map<std::string,std::string> english={
#include "i18n.inc"
};
const char* pages[]={"Наведение","Визуал","Миск","Оружие","Настройки"};
std::vector<const char*> Tabs(int p){switch(p){case 1:return {"Охранники","Объекты","Оформление"};case 2:return {"Скрытность","Игрок","Инструменты"};case 3:return {"Стрельба","Доступ"};case 4:return {"Интерфейс","Профили"};default:return {"Основное"};}}
bool SafeName(const std::string& s){return !s.empty()&&s.size()<64&&s!="."&&s!=".."&&s.find_first_of("/\\\r\n") == std::string::npos;}
void TextAt(ImDrawList* d,ImVec2 p,const char* t,ImU32 color,ImFont* f=nullptr){if(f)d->AddText(f,f->FontSize,p,color,t);else d->AddText(p,color,t);}
}
Menu::Menu(std::filesystem::path dir):profile_dir_(std::move(dir)){
 settings_={
#include "catalogue.inc"
 };Snapshot();if(std::filesystem::exists(profile_dir_/"Default.trace"))Load("Default");
}
std::string Menu::Translate(const std::string& text) const{if(Get("ui.language")==0)return text;auto it=english.find(text);return it==english.end()?text:it->second;}
void Menu::Snapshot(){saved_.clear();for(auto& s:settings_)saved_.push_back(s.value);}
float Menu::Get(const std::string& id) const{for(auto& s:settings_)if(s.id==id)return s.value;return 0;}
bool Menu::Set(const std::string& id,float v){for(auto& s:settings_)if(s.id==id){if(!std::isfinite(v))return false;if(s.kind==Kind::Key){int k=int(v);if(v!=float(k)||k<ImGuiKey_NamedKey_BEGIN||k>=ImGuiKey_NamedKey_END)return false;}else if(v<s.low||v>s.high||((s.kind==Kind::Choice||s.kind==Kind::Toggle)&&v!=std::floor(v)))return false;s.value=v;return true;}return false;}
bool Menu::Dirty()const{for(size_t i=0;i<settings_.size();++i)if(settings_[i].value!=saved_[i])return true;return false;}
void Menu::Reset(){for(auto& s:settings_)s.value=s.initial;}
void Menu::Navigate(int p,int t){page_=std::clamp(p,0,4);auto tabs=Tabs(page_);tab_=std::clamp(t,0,int(tabs.size())-1);}
bool Menu::Save(){
 try{std::filesystem::create_directories(profile_dir_);auto target=profile_dir_/(profile_+".trace");auto tmp=target;tmp+=".tmp";
 std::ofstream f(tmp);f<<"TRACE_OAR_PROFILE 1\n"<<std::setprecision(9);for(auto& s:settings_)f<<std::quoted(s.id)<<' '<<s.value<<'\n';f.close();if(!f){status_="Ошибка записи профиля";return false;}
  
 auto backup=target;backup+=".bak";std::error_code ec;
 if(std::filesystem::exists(target)){std::filesystem::remove(backup,ec);std::filesystem::rename(target,backup);}
 try{std::filesystem::rename(tmp,target);}catch(...){if(std::filesystem::exists(backup))std::filesystem::rename(backup,target);throw;}
 std::filesystem::remove(backup,ec);Snapshot();status_="Профиль сохранён";return true;
 }catch(const std::exception&){status_="Не удалось сохранить профиль";return false;}
}
bool Menu::Load(const std::string& name){
 if(!SafeName(name))return false;std::ifstream f(profile_dir_/(name+".trace"));std::string header;std::getline(f,header);if(header!="TRACE_OAR_PROFILE 1"){status_="Неверный формат профиля";return false;}
 auto previous=settings_;for(auto& s:settings_)s.value=s.initial;
 std::string id;float v;while(f>>std::quoted(id)){if(!(f>>v)){settings_=previous;status_="Повреждённый профиль";return false;}auto known=std::find_if(settings_.begin(),settings_.end(),[&](auto& s){return s.id==id;});if(known!=settings_.end()&&!Set(id,v)){settings_=previous;status_="Некорректное значение профиля";return false;}}
 if(!f.eof()){settings_=previous;return false;}profile_=name;Snapshot();status_="Профиль загружен";return true;
}
bool Menu::CreateProfile(const std::string& name){if(!SafeName(name)||std::filesystem::exists(profile_dir_/(name+".trace"))){status_="Недопустимое или занятое имя";return false;}auto old=profile_;profile_=name;if(Save())return true;profile_=old;return false;}
bool Menu::DeleteProfile(const std::string& name){if(!SafeName(name)||name=="Default"||name==profile_)return false;std::error_code ec;return std::filesystem::remove(profile_dir_/(name+".trace"),ec);}
void Menu::InitializeFonts(std::filesystem::path font_dir){auto& io=ImGui::GetIO();auto ranges=io.Fonts->GetGlyphRangesCyrillic();auto path=font_dir.empty()?std::filesystem::path(TRACE_FONT_DIR):font_dir;ImFontConfig cfg;cfg.OversampleH=2;cfg.OversampleV=2;
 if(std::filesystem::exists(path/"Inter-Regular.otf"))regular_=io.Fonts->AddFontFromFileTTF((path/"Inter-Regular.otf").string().c_str(),11,&cfg,ranges);
 if(std::filesystem::exists(path/"Inter-SemiBold.otf"))bold_=io.Fonts->AddFontFromFileTTF((path/"Inter-SemiBold.otf").string().c_str(),11,&cfg,ranges);
 if(std::filesystem::exists(path/"Inter-SemiBold.otf"))title_=io.Fonts->AddFontFromFileTTF((path/"Inter-SemiBold.otf").string().c_str(),23,&cfg,ranges);
 if(!regular_)regular_=io.Fonts->AddFontDefault();if(!bold_)bold_=regular_;if(!title_)title_=bold_;io.FontDefault=regular_;
}
void Menu::Row(Setting& s){
 bool pending=runtime_mode_&&s.id.rfind("ui.",0)!=0&&!supported_.count(s.id);ImGui::BeginDisabled(pending);
 ImGui::PushID(s.id.c_str());float w=ImGui::GetContentRegionAvail().x;float h=s.kind==Kind::Choice?36:s.kind==Kind::Key?34:s.kind==Kind::Slider?31:28;if(Get("ui.compact"))h-=3;auto start=ImGui::GetCursorScreenPos();auto* d=ImGui::GetWindowDrawList();
 if(s.kind==Kind::Toggle){
  if(ImGui::InvisibleButton("##checkbox",{w,h}))s.value=s.value?0:1;
  TextAt(d,{start.x,start.y+(h-11)/2},Translate(s.label).c_str(),Col(170,162,188));ImVec2 p{start.x+w-14,start.y+(h-14)/2};bool on=s.value!=0;
  d->AddRectFilled(p,{p.x+14,p.y+14},on?Accent():Col(25,24,34),4);d->AddRect(p,{p.x+14,p.y+14},on?Accent():Col(83,69,98),4);
  if(on){d->AddLine({p.x+4,p.y+7},{p.x+6,p.y+9},Col(49,34,61),1.5f);d->AddLine({p.x+6,p.y+9},{p.x+10,p.y+5},Col(49,34,61),1.5f);}
 }else if(s.kind==Kind::Slider){
  float label_width=137, value_width=50, track_width=w-label_width-value_width-10;TextAt(d,{start.x,start.y+(h-11)/2},Translate(s.label).c_str(),Col(170,162,188));
  char value[64];if(s.high>=100)std::snprintf(value,sizeof(value),"%.0f%s",s.value,Translate(s.unit).c_str());else std::snprintf(value,sizeof(value),"%.1f%s",s.value,Translate(s.unit).c_str());auto size=ImGui::CalcTextSize(value);TextAt(d,{start.x+w-size.x,start.y+(h-11)/2},value,Accent());
  float progress=(s.value-s.low)/(s.high-s.low),x=start.x+label_width,cy=start.y+h/2;
  d->AddLine({x,cy},{x+track_width,cy},Col(70,64,82),3);d->AddLine({x,cy},{x+track_width*progress,cy},Accent(),3);d->AddCircleFilled({x+track_width*progress,cy},4,Col(37,32,48));d->AddCircle({x+track_width*progress,cy},3.5f,Accent(),0,1.5f);
  ImGui::SetCursorScreenPos({x,start.y+(h-13)/2});ImGui::SetNextItemWidth(track_width);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,0});
  for(auto col:{ImGuiCol_Text,ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive})ImGui::PushStyleColor(col,ImVec4(0,0,0,0));ImGui::SliderFloat("##value",&s.value,s.low,s.high,"");ImGui::PopStyleColor(6);ImGui::PopStyleVar();
 }else if(s.kind==Kind::Choice){
  ImGui::SetCursorScreenPos({start.x,start.y+3});ImGui::SetNextItemWidth(w);int n=int(s.value);std::string preview=Translate(s.label)+": "+Translate(s.choices[n]);if(ImGui::BeginCombo("##choice",preview.c_str())){for(int i=0;i<int(s.choices.size());++i)if(ImGui::Selectable(Translate(s.choices[i]).c_str(),n==i))s.value=float(i);ImGui::EndCombo();}
 }else{
  TextAt(d,{start.x,start.y+(h-11)/2},Translate(s.label).c_str(),Col(170,162,188));ImGui::SetCursorScreenPos({start.x+w-78,start.y+5});auto key=ImGuiKey(int(s.value));bool listening=binding_==int(&s-settings_.data());if(ImGui::Button(listening?"...":ImGui::GetKeyName(key),{78,24}))binding_=int(&s-settings_.data());
 }
 ImGui::SetCursorScreenPos({start.x,start.y+h});ImGui::Dummy({w,0});ImGui::PopID();ImGui::EndDisabled();
}
void Menu::Profiles(){
 ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,10});
 ImGui::SetNextItemWidth(260);ImGui::InputTextWithHint("##name",Translate("Название нового профиля").c_str(),profile_name_,sizeof(profile_name_));ImGui::SameLine();if(ImGui::Button(Translate("Создать +").c_str()))CreateProfile(profile_name_);
 std::vector<std::string> names={"Default"};std::error_code ec;if(std::filesystem::exists(profile_dir_))for(auto& entry:std::filesystem::directory_iterator(profile_dir_,ec))if(entry.path().extension()==".trace"&&entry.path().stem()!="Default")names.push_back(entry.path().stem().string());
 std::sort(names.begin()+1,names.end());for(auto& name:names){ImGui::PushID(name.c_str());ImGui::Spacing();ImGui::Separator();ImGui::TextUnformatted(name.c_str());ImGui::SameLine(310);if(name==profile_)ImGui::TextColored(accent,Translate("АКТИВНЫЙ").c_str());else{if(ImGui::Button(Translate("Загрузить").c_str())){if(Dirty()){std::snprintf(profile_name_,sizeof(profile_name_),"%s",name.c_str());ImGui::OpenPopup("unsaved");}else Load(name);}ImGui::SameLine();if(name!="Default"&&ImGui::Button(Translate("Удалить").c_str()))DeleteProfile(name);}
 if(ImGui::BeginPopupModal((Translate("Несохранённые изменения")+"###unsaved").c_str(),nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted(Translate("Сохранить текущий профиль перед загрузкой?").c_str());if(ImGui::Button(Translate("Сохранить и загрузить").c_str())){if(Save()&&Load(profile_name_))ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button(Translate("Без сохранения").c_str())){Load(profile_name_);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button(Translate("Отмена").c_str()))ImGui::CloseCurrentPopup();ImGui::EndPopup();}ImGui::PopID();}
 ImGui::PopStyleVar();
}
void Menu::Render(){
 auto& io=ImGui::GetIO();bool was_binding=binding_>=0;
 if(binding_>=0){if(ImGui::IsKeyPressed(ImGuiKey_Escape))binding_=-1;else for(int k=ImGuiKey_NamedKey_BEGIN;k<ImGuiKey_NamedKey_END;++k)if(ImGui::IsKeyPressed(ImGuiKey(k))&&k!=ImGuiKey_MouseLeft&&k!=ImGuiKey_LeftCtrl&&k!=ImGuiKey_RightCtrl){settings_[binding_].value=float(k);binding_=-1;break;}}
 if(!was_binding&&!io.WantTextInput&&(ImGui::IsKeyPressed(ImGuiKey(int(Get("ui.menu_key"))))||ImGui::IsKeyPressed(ImGuiKey_End)))visible=!visible;
 if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_S))Save();if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_K)){visible=true;search_open_=true;}
 if(!visible)return;
 int color=int(Get("ui.accent"));accent=color==1?ImVec4(.53f,.78f,.68f,1):color==2?ImVec4(.98f,.53f,.36f,1):ImVec4(181.f/255,166.f/255,246.f/255,1);
 struct RestoreStyle {ImGuiStyle previous=ImGui::GetStyle();~RestoreStyle(){ImGui::GetStyle()=previous;}} restore_style;
 auto& st=ImGui::GetStyle();st.WindowPadding={0,0};st.WindowRounding=12;st.WindowBorderSize=1;st.ChildRounding=7;st.ChildBorderSize=1;st.FrameRounding=4;st.FrameBorderSize=1;st.FramePadding={9,6};st.ItemSpacing={8,0};st.PopupRounding=6;st.GrabRounding=4;st.ScrollbarSize=5;
 st.Colors[ImGuiCol_WindowBg]={27.f/255,28.f/255,36.f/255,Get("ui.opacity")};st.Colors[ImGuiCol_ChildBg]={35.f/255,35.f/255,47.f/255,1};st.Colors[ImGuiCol_Border]={58.f/255,53.f/255,73.f/255,1};st.Colors[ImGuiCol_Text]={.83f,.80f,.9f,1};st.Colors[ImGuiCol_FrameBg]={.106f,.102f,.149f,1};st.Colors[ImGuiCol_FrameBgHovered]={.18f,.15f,.23f,1};st.Colors[ImGuiCol_FrameBgActive]={.22f,.18f,.28f,1};st.Colors[ImGuiCol_Button]={.188f,.149f,.22f,1};st.Colors[ImGuiCol_ButtonHovered]={.23f,.165f,.28f,1};st.Colors[ImGuiCol_ButtonActive]={.28f,.20f,.34f,1};st.Colors[ImGuiCol_SliderGrab]=accent;st.Colors[ImGuiCol_SliderGrabActive]=accent;st.Colors[ImGuiCol_CheckMark]=accent;st.Colors[ImGuiCol_Header]={.22f,.19f,.28f,1};st.Colors[ImGuiCol_HeaderHovered]={.28f,.22f,.33f,1};st.Colors[ImGuiCol_PopupBg]={.15f,.125f,.21f,1};st.Colors[ImGuiCol_ScrollbarBg]={0,0,0,0};st.Colors[ImGuiCol_ScrollbarGrab]={.41f,.34f,.53f,1};
 ImGui::SetNextWindowSize({880,610},ImGuiCond_Always);ImGui::SetNextWindowPos({(io.DisplaySize.x-880)/2,(io.DisplaySize.y-610)/2},ImGuiCond_FirstUseEver);
 ImGui::Begin("trace / One-armed Robber",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
 auto p=ImGui::GetWindowPos();auto* d=ImGui::GetWindowDrawList();d->AddRectFilled({p.x+1,p.y+1},{p.x+879,p.y+56},Col(32,32,41),12,ImDrawFlags_RoundCornersTop);d->AddLine({p.x,p.y+56},{p.x+880,p.y+56},Col(52,51,63));
 d->AddText(title_,26,{p.x+24,p.y+15},Col(236,233,244),"trace");float brand_width=title_->CalcTextSizeA(26,1000,0,"trace").x;d->AddText(title_,26,{p.x+24+brand_width,p.y+15},Accent(),".");
 ImGui::SetCursorPos({740,17});if(ImGui::Button(Get("ui.language")==0?"RU / EN":"EN / RU",{78,23}))Set("ui.language",Get("ui.language")==0?1:0);
 ImGui::SetCursorPos({830,16});if(ImGui::InvisibleButton("close",{28,24}))visible=false;d->AddText(title_,21,{p.x+837,p.y+16},ImGui::IsItemHovered()?Col(233,220,255):Col(154,147,170),"×");
 d->AddRectFilled({p.x+1,p.y+57},{p.x+76,p.y+609},Col(21,22,30),12,ImDrawFlags_RoundCornersBottomLeft);d->AddLine({p.x+76,p.y+56},{p.x+76,p.y+610},Col(50,48,61));
 const char* nav[]={"Аим","Визуал","Миск","Оружие","Настройки"};
 auto icon=[&](int i,ImVec2 c,ImU32 col){float x=c.x,y=c.y;
 if(i==0){d->AddCircle(c,7,col,24,1.2f);d->AddCircle(c,3,col,16,1.2f);d->AddLine({x-10,y},{x-5,y},col);d->AddLine({x+5,y},{x+10,y},col);d->AddLine({x,y-10},{x,y-5},col);d->AddLine({x,y+5},{x,y+10},col);}
 else if(i==1){d->AddBezierCubic({x-9,y},{x-3,y-9},{x+3,y-9},{x+9,y},col,1.2f);d->AddBezierCubic({x-9,y},{x-3,y+9},{x+3,y+9},{x+9,y},col,1.2f);d->AddCircle(c,2.5f,col,16,1.2f);}
 else if(i==2){d->AddLine({x-7,y+7},{x+4,y-4},col,2);d->AddCircle({x+4,y-4},4,col,12,1.2f);d->AddCircle({x-7,y+7},1.5f,col,12);}
 else if(i==3){d->AddQuad({x,y-9},{x+7,y-4},{x,y+1},{x-7,y-4},col);d->AddLine({x-7,y-4},{x-7,y+4},col);d->AddLine({x+7,y-4},{x+7,y+4},col);d->AddLine({x,y+1},{x,y+9},col);d->AddLine({x-7,y+4},{x,y+9},col);d->AddLine({x+7,y+4},{x,y+9},col);}
 else{d->AddCircle(c,6,col,16,1.2f);d->AddCircle(c,2,col,12,1.2f);for(int k=0;k<8;++k){float a=k*3.14159265f/4;d->AddLine({x+7*std::cos(a),y+7*std::sin(a)},{x+9*std::cos(a),y+9*std::sin(a)},col,1.2f);}}
 };
 for(int i=0;i<5;++i){float y=74+i*60+(i>=2?24:0);ImGui::SetCursorPos({9,y});ImGui::PushID(i);if(ImGui::InvisibleButton("nav",{58,54}))Navigate(i);bool active=page_==i;if(active||ImGui::IsItemHovered())d->AddRectFilled({p.x+9,p.y+y},{p.x+67,p.y+y+54},active?Col(48,42,66):Col(37,33,51),8);if(active)d->AddRectFilled({p.x,p.y+y+20},{p.x+2,p.y+y+35},Accent(),2);auto col=active?Col(224,216,255):Col(119,115,138);icon(i,{p.x+38,p.y+y+19},col);auto size=ImGui::CalcTextSize(Translate(nav[i]).c_str());TextAt(d,{p.x+38-size.x/2,p.y+y+36},Translate(nav[i]).c_str(),col);ImGui::PopID();}
 d->AddLine({p.x+9,p.y+204},{p.x+67,p.y+204},Col(48,43,59));
 TextAt(d,{p.x+98,p.y+76},Translate(pages[page_]).c_str(),Col(233,229,244),title_);
 auto tabs=Tabs(page_);ImGui::SetCursorPos({98,120});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4,0});for(int i=0;i<int(tabs.size());++i){if(i)ImGui::SameLine();ImGui::PushStyleColor(ImGuiCol_Button,i==tab_?ImVec4(.224f,.188f,.278f,1):ImVec4(.078f,.082f,.114f,1));if(ImGui::Button(Translate(tabs[i]).c_str(),{0,30}))tab_=i;ImGui::PopStyleColor();}ImGui::PopStyleVar();
 bool reset_requested=false;
 ImGui::SetCursorPos({98,166});ImGui::PushStyleColor(ImGuiCol_ChildBg,{0,0,0,0});ImGui::BeginChild("content",{758,422},ImGuiChildFlags_None);ImGui::PopStyleColor();
 if(page_==4&&tab_==1){ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{15,13});ImGui::BeginChild("profile_card",{748,380},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding);Profiles();ImGui::Spacing();if(ImGui::Button(Translate("Сохранить профиль").c_str(),{170,30}))Save();ImGui::SameLine();if(ImGui::Button(Translate("Сбросить настройки").c_str(),{170,30}))reset_requested=true;if(!status_.empty()){ImGui::Spacing();ImGui::TextUnformatted(Translate(status_).c_str());}ImGui::EndChild();ImGui::PopStyleVar();}
 else{
  struct Card{std::string title;std::vector<Setting*> items;};std::vector<Card> cards[2];
  auto add=[&](int col,const std::string& group,Setting* s){if(cards[col].empty()||cards[col].back().title!=group)cards[col].push_back({group,{}});cards[col].back().items.push_back(s);};
  if(page_==0){
   for(auto& s:settings_)if(s.page==0){bool target=s.id=="aim.bone"||s.id=="aim.visible"||s.id=="aim.alive"||s.id=="aim.distance"||s.id=="aim.priority";add(target?1:0,target?"Цель":"Основное",&s);}
   for(auto& s:settings_)if(s.id=="weapon.no_recoil"||s.id=="weapon.no_spread")add(0,"Контроль оружия",&s);
   for(auto& s:settings_)if(s.id=="esp.guard.box"||s.id=="esp.guard.health")add(1,"Отображение цели",&s);
  }else for(auto& s:settings_)if(s.page==page_&&s.tab==tab_)add(s.column,s.group,&s);
  float y=0;size_t count=std::max(cards[0].size(),cards[1].size());
  for(size_t row=0;row<count;++row){float height=0;for(int col=0;col<2;++col)if(row<cards[col].size()){float needed=52;for(auto* s:cards[col][row].items)needed+=s->kind==Kind::Choice?36:s->kind==Kind::Key?34:s->kind==Kind::Slider?31:28;height=std::max(height,needed);}
   for(int col=0;col<2;++col)if(row<cards[col].size()){ImGui::SetCursorPos({col?380.f:0.f,y});ImGui::PushID(int(row*2+col));ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{15,13});ImGui::BeginChild("card",{368,height},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);ImGui::PushFont(bold_);ImGui::TextColored({.80f,.765f,.894f,1},"%s",Translate(cards[col][row].title).c_str());ImGui::PopFont();ImGui::Dummy({0,10});for(auto* s:cards[col][row].items)Row(*s);ImGui::EndChild();ImGui::PopStyleVar();ImGui::PopID();}y+=height+12;
  }
  if(page_==4){ImGui::SetCursorPos({0,y});if(ImGui::Button(Translate("Сохранить профиль").c_str(),{170,30}))Save();ImGui::SameLine();if(ImGui::Button(Translate("Сбросить настройки").c_str(),{170,30}))reset_requested=true;if(!status_.empty())ImGui::TextUnformatted(Translate(status_).c_str());}
 }
 ImGui::EndChild();if(reset_requested)ImGui::OpenPopup("reset");
 if(ImGui::BeginPopupModal((Translate("Сбросить настройки?")+"###reset").c_str(),nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted(Translate("Вернуть настройки к начальным значениям?").c_str());if(ImGui::Button(Translate("Сбросить").c_str())){Reset();ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button(Translate("Отмена").c_str()))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 if(search_open_){ImGui::OpenPopup("search");search_open_=false;query_[0]=0;}
 ImGui::SetNextWindowSize({540,420},ImGuiCond_Appearing);if(ImGui::BeginPopupModal((Translate("Найти настройку")+"###search").c_str(),nullptr,ImGuiWindowFlags_NoResize)){ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##search",Translate("Поиск по всем разделам").c_str(),query_,sizeof(query_));ImGui::BeginChild("results",{0,300});for(auto& s:settings_)if(!query_[0]||Translate(s.label).find(query_)!=std::string::npos||s.id.find(query_)!=std::string::npos){ImGui::PushID(s.id.c_str());if(ImGui::Selectable((Translate(s.label)+" / "+Translate(pages[s.page])).c_str())){Navigate(s.page,s.tab);ImGui::CloseCurrentPopup();}ImGui::PopID();}ImGui::EndChild();if(ImGui::Button(Translate("Закрыть").c_str()))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 ImGui::End();
}

}
