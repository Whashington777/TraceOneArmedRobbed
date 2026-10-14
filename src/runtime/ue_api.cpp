#include "ue_api.hpp"
#include <algorithm>
#include <cstring>
namespace trace::runtime {
bool Readable(const void* p,size_t size,bool writable){if(!p||!size)return false;uintptr_t end=reinterpret_cast<uintptr_t>(p)+size;if(end<reinterpret_cast<uintptr_t>(p))return false;auto at=reinterpret_cast<uintptr_t>(p);while(at<end){MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(reinterpret_cast<void*>(at),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;DWORD bits=info.Protect&0xff;if(writable&&bits!=PAGE_READWRITE&&bits!=PAGE_WRITECOPY&&bits!=PAGE_EXECUTE_READWRITE&&bits!=PAGE_EXECUTE_WRITECOPY)return false;auto next=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(next<=at)return false;at=next;}return true;}
bool UEApi::Bind(){auto dll=GetModuleHandleW(L"UE4SS.dll");if(!dll)return false;
#define SYM(member,symbol) member=reinterpret_cast<decltype(member)>(GetProcAddress(dll,symbol));if(!member)return false
 SYM(cls_,"?GetClassPrivate@UObjectBase@Unreal@RC@@QEAAAEAPEAVUClass@23@XZ");
 SYM(name_,"?GetNamePrivate@UObjectBase@Unreal@RC@@QEAAAEAUFName@23@XZ");
 SYM(flags_,"?GetObjectFlags@UObjectBase@Unreal@RC@@QEAAAEAW4EObjectFlags@23@XZ");
 SYM(index_,"?GetInternalIndex@UObjectBase@Unreal@RC@@QEAAHXZ");
 SYM(comparison_,"?GetComparisonIndex@FName@Unreal@RC@@QEBAIXZ");
 SYM(number_,"?GetNumber@FName@Unreal@RC@@QEBAIXZ");
 SYM(property_,"?GetPropertyByNameInChain@UObject@Unreal@RC@@QEAAPEAVFProperty@23@PEB_W@Z");
 SYM(function_,"?GetFunctionByNameInChain@UObject@Unreal@RC@@QEAAPEAVUFunction@23@PEB_W@Z");
 SYM(offset_,"?GetOffset_Internal@FProperty@Unreal@RC@@QEAAAEAHXZ");
 SYM(size_,"?GetElementSize@FProperty@Unreal@RC@@QEAAAEAHXZ");
 SYM(param_size_,"?GetParmsSize@UFunction@Unreal@RC@@QEAAAEAGXZ");
 SYM(return_offset_,"?GetReturnValueOffset@UFunction@Unreal@RC@@QEAAAEAGXZ");
 SYM(event_,"?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z");
 SYM(find_,"?StaticFindObject_InternalSlow@UObjectGlobals@Unreal@RC@@YAPEAVUObject@23@PEAVUClass@23@PEAV423@PEB_W_N@Z");
 SYM(script_,"?GetScript@UStruct@Unreal@RC@@QEAAAEAV?$TArray@EV?$TSizedDefaultAllocator@$0CA@@Unreal@RC@@@23@XZ");
 SYM(bool_get_,"?GetPropertyValue@FBoolProperty@Unreal@RC@@QEAA_NPEBX@Z");
 SYM(bool_set_,"?SetPropertyValue@FBoolProperty@Unreal@RC@@QEAAXPEAX_N@Z");
 SYM(cdo_,"?GetClassDefaultObject@UClass@Unreal@RC@@QEAAAEAPEAVUObject@23@XZ");
#undef SYM
 return true;}
bool UEApi::Valid(void* o){if(!cls_||!Readable(o,40))return false;auto f=static_cast<uint32_t*>(flags_(o));if(!Readable(f,4)||(*f&(0x8000u|0x10000u)))return false;auto c=static_cast<void**>(cls_(o));return Readable(c,8)&&Readable(*c,40)&&index_(o)>=0;}
Identity UEApi::Id(void* o){if(!Valid(o))return {};auto n=name_(o);return {*static_cast<void**>(cls_(o)),comparison_(n),number_(n),index_(o)};}
void* UEApi::Ptr(void* o,const wchar_t* name,int bytes){if(!Valid(o))return nullptr;void* p=property_(o,name);if(!p)return nullptr;auto n=size_(p);auto off=offset_(p);if(!Readable(n,4)||!Readable(off,4)||*n!=bytes||*off<0||*off>2*1024*1024)return nullptr;auto ptr=static_cast<uint8_t*>(o)+*off;return Readable(ptr,bytes,true)?ptr:nullptr;}
void* UEApi::Object(void* o,const wchar_t* name){auto p=Ptr(o,name,8);if(!p)return nullptr;void* obj{};std::memcpy(&obj,p,8);return Valid(obj)?obj:nullptr;}
void* UEApi::Find(const wchar_t* path){return find_?find_(nullptr,nullptr,path,false):nullptr;}
Array* UEApi::Script(void* fn){if(!Valid(fn))return nullptr;auto a=static_cast<Array*>(script_(fn));if(!Readable(a,sizeof(Array))||a->num<3||a->num>1024*1024||a->capacity<a->num||!Readable(a->data,a->num,true))return nullptr;return a;}
void* UEApi::DefaultObject(void* o){if(!Valid(o))return nullptr;auto c=*static_cast<void**>(cls_(o));auto ptr=cdo_(c);return Readable(ptr,8)?*static_cast<void**>(ptr):nullptr;}
bool UEApi::BoolCall(void* o,const wchar_t* name,bool& result){if(!Valid(o))return false;auto f=function_(o,name);if(!Valid(f))return false;int len=*param_size_(f);if(len<1||len>1024)return false;int off=*return_offset_(f);if(off<0||off>=len)return false;std::vector<uint8_t> params(len);event_(o,f,params.data());result=params[off]!=0;return true;}
bool UEApi::BoolField(void* o,const wchar_t* name,bool& value,bool write){if(!Valid(o))return false;auto property=property_(o,name);auto data=Ptr(o,name,1);if(!property||!data)return false;if(write)bool_set_(property,data,value);else value=bool_get_(property,data);return true;}
bool UEApi::BoolArgument(void* o,const wchar_t* name,bool value){if(!Valid(o))return false;auto f=function_(o,name);if(!Valid(f))return false;auto n=*param_size_(f);if(n<1||n>4)return false;uint8_t params[4]={uint8_t(value),0,0,0};event_(o,f,params);return true;}
bool UEApi::SetMovementMode(void* o,uint8_t mode){if(!Valid(o))return false;auto f=function_(o,L"SetMovementMode");if(!Valid(f))return false;auto n=*param_size_(f);if(n<2||n>4)return false;uint8_t params[4]={mode,0,0,0};event_(o,f,params);return true;}
bool UEApi::AddVerticalInput(void* o,float scale){if(!Valid(o))return false;auto f=function_(o,L"AddMovementInput");if(!Valid(f))return false;auto n=*param_size_(f);if(n<17||n>24)return false;uint8_t params[24]{};float direction[3]={0,0,1};std::memcpy(params,direction,12);std::memcpy(params+12,&scale,4);event_(o,f,params);return true;}
void* UEApi::ClassDefault(void* c){if(!Valid(c))return nullptr;auto p=cdo_(c);return Readable(p,8)?*static_cast<void**>(p):nullptr;}
bool UEApi::Call(void* o,const wchar_t* name,std::initializer_list<Argument> args){
 if(!Valid(o))return false;auto f=function_(o,name);if(!Valid(f))return false;
 int len=*param_size_(f);if(len<0||len>4096)return false;std::vector<uint8_t> params(std::max(1,len));
 struct Slot{Argument arg;int offset;};std::vector<Slot> slots;
 for(auto arg:args){auto p=property_(f,arg.name);if(!p)return false;int n=*size_(p),off=*offset_(p);if(n!=arg.size||off<0||off+n>len||!arg.data)return false;slots.push_back({arg,off});if(!arg.output)std::memcpy(params.data()+off,arg.data,n);}
 event_(o,f,params.data());for(auto& slot:slots)if(slot.arg.output)std::memcpy(slot.arg.data,params.data()+slot.offset,slot.arg.size);return true;
}

}
