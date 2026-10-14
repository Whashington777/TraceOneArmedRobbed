#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
namespace trace::runtime {
struct Array { void* data; int num; int capacity; };
struct Identity {void* cls{}; uint32_t name{};uint32_t number{};int index{};bool operator==(const Identity& x)const{return cls==x.cls&&name==x.name&&number==x.number&&index==x.index;}};
class UEApi {
public:
 struct Argument { const wchar_t* name; void* data; int size; bool output=false; };
 bool Call(void* object,const wchar_t* name,std::initializer_list<Argument> arguments);
 void* ClassDefault(void* cls);
 bool Bind();
 bool Valid(void* object);
 Identity Id(void* object);
 void* Ptr(void* object,const wchar_t* name,int size=4);
 void* Object(void* object,const wchar_t* name);
 bool BoolCall(void* object,const wchar_t* name,bool& result);
 bool BoolField(void* object,const wchar_t* name,bool& value,bool write=false);
 bool BoolArgument(void* object,const wchar_t* name,bool value);
 bool SetMovementMode(void* movement,uint8_t mode);
 bool AddVerticalInput(void* player,float scale);
 void* Find(const wchar_t* path);
 Array* Script(void* function);
 void* DefaultObject(void* object);
private:
 using Unary=void*(*)(void*);using FindType=void*(*)(void*,void*,const wchar_t*,bool);
 using PropertyType=void*(*)(void*,const wchar_t*);using IntRef=int*(*)(void*);using NameIndex=uint32_t(*)(void*);using Event=void(*)(void*,void*,void*);
 bool(*bool_get_)(void*,const void*){};void(*bool_set_)(void*,void*,bool){};
 Unary cls_{},name_{},flags_{},script_{},cdo_{};
 PropertyType property_{},function_{};IntRef offset_{},size_{};uint16_t*(*param_size_)(void*){};uint16_t*(*return_offset_)(void*){};NameIndex comparison_{},number_{};int(*index_)(void*){};Event event_{};FindType find_{};
};
bool Readable(const void* p,size_t size,bool writable=false);
}
