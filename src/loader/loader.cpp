#include <cerrno>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <chrono>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
namespace fs=std::filesystem;
static std::string Read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static std::vector<std::string> Split(const std::string& bytes){std::vector<std::string> out;size_t start=0;while(start<bytes.size()){auto end=bytes.find('\0',start);if(end==std::string::npos)end=bytes.size();if(end>start)out.push_back(bytes.substr(start,end-start));start=end+1;}return out;}
static bool Game(int pid){auto cmd=Split(Read(fs::path("/proc")/std::to_string(pid)/"cmdline"));if(cmd.empty())return false;auto name=cmd.front();auto slash=name.find_last_of("/\\");if(slash!=std::string::npos)name=name.substr(slash+1);return name=="OAR-Win64-Shipping.exe";}
static std::string Env(const std::vector<std::string>& env,const std::string& key){for(auto& s:env)if(s.rfind(key+"=",0)==0)return s.substr(key.size()+1);return {};}
static fs::path GameDirectory(const fs::path& proc,const std::vector<std::string>& environment){
 auto install=Env(environment,"STEAM_COMPAT_INSTALL_PATH");if(!install.empty()){auto p=fs::path(install)/"OAR/Binaries/Win64";if(fs::is_regular_file(p/"UE4SS.dll"))return p;}
 std::istringstream maps(Read(proc/"maps"));std::string line;while(std::getline(maps,line)){auto slash=line.find('/');if(slash==std::string::npos)continue;fs::path path=line.substr(slash);if(path.filename()=="UE4SS.dll"||path.filename()=="OAR-Win64-Shipping.exe"){auto folder=path.parent_path();if(fs::is_regular_file(folder/"UE4SS.dll"))return folder;}}
 return {};
}
static int PrepareBootstrap(const fs::path& game,bool check){
 auto own=fs::read_symlink("/proc/self/exe").parent_path();auto source=own/"bootstrap/main.lua";if(!fs::is_regular_file(source)){std::cerr<<"Loader bootstrap missing: "<<source<<"\n";return 2;}
 auto mod=game/"Mods/TraceOAR",script=mod/"Scripts/main.lua",mods=game/"Mods/mods.txt";std::string wanted=Read(source),existing=Read(script),settings=Read(mods);std::istringstream lines(settings);std::ostringstream updated;std::string line;bool found=false;
 while(std::getline(lines,line)){if(std::regex_match(line,std::regex("^[ \t]*TraceOAR[ \t]*:[ \t]*[0-9]+[ \t\r]*$"))){updated<<"TraceOAR : 1\n";found=true;}else updated<<line<<'\n';}if(!found)updated<<"TraceOAR : 1\n";
 if(wanted!=existing||updated.str()!=settings){if(check){std::cerr<<"Bootstrap needs installation. Run without --check, then restart the game.\n";return 3;}
  fs::create_directories(script.parent_path());auto temporary=script;temporary+=".new";fs::copy_file(source,temporary,fs::copy_options::overwrite_existing);fs::rename(temporary,script);{std::ofstream f(mods);f<<updated.str();}
  for(auto name:{"attach.request","attach.response","attach.ready"})fs::remove(mod/name);
  std::cout<<"Bootstrap installed without DLL autoload. Restart the game, then run the loader again.\n";return 3;
 }
 return 0;
}
static bool Recent(const fs::path& path){if(!fs::is_regular_file(path))return false;return fs::file_time_type::clock::now()-fs::last_write_time(path)<std::chrono::seconds(3);}
static bool CurrentRequest(const fs::path& proc,const fs::path& request){
 if(!fs::is_regular_file(request))return false;auto stat=Read(proc/"stat");auto end=stat.rfind(')');if(end==std::string::npos)return false;std::istringstream fields(stat.substr(end+2));std::string field;double start=0;for(int index=3;index<=22;++index){if(!(fields>>field))return false;if(index==22)start=std::stod(field)/sysconf(_SC_CLK_TCK);}
 double uptime=0;std::ifstream("/proc/uptime")>>uptime;auto launched=std::chrono::system_clock::now()-std::chrono::milliseconds(static_cast<long long>((uptime-start)*1000));auto modified=fs::last_write_time(request)-fs::file_time_type::clock::now()+std::chrono::system_clock::now();return modified>=launched-std::chrono::seconds(1);
}
extern "C" int trace_load(int pid,const char* dll,const char* injector,int check){try{
 if(!pid){for(auto& p:fs::directory_iterator("/proc")){auto name=p.path().filename().string();if(name.find_first_not_of("0123456789")!=std::string::npos)continue;int candidate=std::stoi(name);if(Game(candidate)){if(pid){std::cerr<<"Multiple games running. Use --pid.\n";return 2;}pid=candidate;}}}
 if(!pid||!Game(pid)){std::cerr<<"Start One-armed Robber first.\n";return 2;}
 auto proc=fs::path("/proc")/std::to_string(pid);auto environment=Split(Read(proc/"environ"));auto prefix=Env(environment,"WINEPREFIX");if(prefix.empty()){std::cerr<<"Game has no Wine prefix.\n";return 2;}
 auto wine=Env(environment,"WINELOADER");if(wine.empty()||!fs::is_regular_file(wine)){wine=fs::read_symlink(proc/"exe").string();auto tail=wine.rfind("-preloader");if(tail!=std::string::npos&&tail+10==wine.size())wine.resize(tail);}
 if(!fs::is_regular_file(wine)||access(wine.c_str(),X_OK)){std::cerr<<"Cannot find the game's Wine loader.\n";return 2;}
 auto library=fs::absolute(dll),helper=fs::absolute(injector);if(!fs::is_regular_file(library)||!fs::is_regular_file(helper)){std::cerr<<"TraceRuntime.dll or TraceInject.exe missing.\n";return 2;}
 auto game=GameDirectory(proc,environment);if(game.empty()){std::cerr<<"UE4SS installation not found in the running game.\n";return 2;}auto bootstrap=PrepareBootstrap(game,check);if(bootstrap)return bootstrap;
 auto bridge=game/"Mods/TraceOAR";if(!CurrentRequest(proc,bridge/"attach.request")){std::cerr<<"Bootstrap is not running in this game session. Restart the game.\n";return 3;}
 auto challenge=Read(bridge/"attach.request");if(challenge.empty()||challenge.size()>256){std::cerr<<"Invalid bootstrap session.\n";return 3;}
 if(check){std::cout<<"Game PID: "<<pid<<"\nWine: "<<wine<<"\nDLL: "<<library<<"\nReady for injection.\n";return 0;}
 std::string windows_dll="Z:"+library.string(),windows_helper="Z:"+helper.string();for(auto* s:{&windows_dll,&windows_helper})for(auto& c:*s)if(c=='/')c='\\';
 environment.erase(std::remove_if(environment.begin(),environment.end(),[](const std::string& value){auto key=value.substr(0,value.find('='));return key=="WINESERVERSOCKET"||key=="WINELOADERNOEXEC"||key=="WINEPRELOADRESERVE"||key=="WINE_LD_PRELOAD";}),environment.end());
 std::vector<char*> env;for(auto& s:environment)env.push_back(s.data());env.push_back(nullptr);
 pid_t child=fork();if(child<0){std::cerr<<"fork failed.\n";return 1;}if(!child){char* args[]={wine.data(),windows_helper.data(),windows_dll.data(),nullptr};execve(wine.c_str(),args,env.data());_exit(127);}
 int status=0;while(waitpid(child,&status,0)<0){if(errno!=EINTR){std::cerr<<"waitpid failed.\n";return 1;}}if(!WIFEXITED(status)||WEXITSTATUS(status)!=0)return WIFEXITED(status)?WEXITSTATUS(status):1;
 for(int attempt=0;attempt<48;++attempt){if(Read(bridge/"attach.ready")==challenge&&Recent(bridge/"attach.ready")){std::cout<<"Gameplay bridge connected to the local host.\n";return 0;}usleep(250000);}
 std::cerr<<"DLL loaded, but local-host gameplay Tick is not connected. Enter a local host level and run the loader again.\n";return 4;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
