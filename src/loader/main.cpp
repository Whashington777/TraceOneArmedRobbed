#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
extern "C" int trace_load(int,const char*,const char*,int);
int main(int argc,char** argv){try{
 auto own=std::filesystem::read_symlink("/proc/self/exe").parent_path();auto mod=own/"../Win64/Mods/TraceOAR";std::string dll=(mod/"TraceRuntime.dll").string(),injector=(mod/"TraceInject.exe").string();int pid=0,check=0;
 for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--help"){std::cout<<"TraceLoader [--pid LINUX_PID] [--dll PATH] [--injector PATH] [--check]\n";return 0;}else if(arg=="--check")check=1;else if((arg=="--pid"||arg=="--dll"||arg=="--injector")&&i+1<argc){std::string value=argv[++i];if(arg=="--pid"){size_t n=0;pid=std::stoi(value,&n);if(n!=value.size()||pid<=0)return 2;}else if(arg=="--dll")dll=value;else injector=value;}else{std::cerr<<"Invalid argument: "<<arg<<'\n';return 2;}}
 return trace_load(pid,dll.c_str(),injector.c_str(),check);
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
