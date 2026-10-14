#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <cstdint>
struct Handle {HANDLE h{};~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}operator HANDLE()const{return h;}};
static DWORD FindGame(){Handle list{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};PROCESSENTRY32W e{sizeof(e)};DWORD pid=0;int count=0;if(Process32FirstW(list,&e))do{if(_wcsicmp(e.szExeFile,L"OAR-Win64-Shipping.exe")==0){pid=e.th32ProcessID;++count;}}while(Process32NextW(list,&e));return count==1?pid:0;}
static LPTHREAD_START_ROUTINE RemoteLoader(DWORD pid){
 auto fn=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");HMODULE owner{};if(!fn||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(fn),&owner))return nullptr;
 wchar_t path[32768]{};GetModuleFileNameW(owner,path,32768);auto name=std::filesystem::path(path).filename().wstring();Handle list{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid)};MODULEENTRY32W entry{sizeof(entry)};
 if(Module32FirstW(list,&entry))do{if(_wcsicmp(entry.szModule,name.c_str())==0){auto rva=reinterpret_cast<uintptr_t>(fn)-reinterpret_cast<uintptr_t>(owner);return reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<uintptr_t>(entry.modBaseAddr)+rva);}}while(Module32NextW(list,&entry));return nullptr;
}
int wmain(int argc,wchar_t** argv){
 if(argc>3){std::wcerr<<L"Usage: TraceInject.exe [TraceRuntime.dll] [PID]\n";return 2;}
 wchar_t own[32768]{};GetModuleFileNameW(nullptr,own,32768);auto path=argc>=2?std::filesystem::absolute(argv[1]):std::filesystem::path(own).parent_path()/L"TraceRuntime.dll";if(!std::filesystem::exists(path)){std::wcerr<<L"DLL not found: "<<path<<L"\n";return 2;}
 DWORD pid=0;if(argc==3){try{pid=DWORD(std::stoul(argv[2]));}catch(...){return 2;}}else pid=FindGame();if(!pid){std::wcerr<<L"Start One-armed Robber first, or specify the one game PID.\n";return 2;}
 Handle process{OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid)};if(!process){std::wcerr<<L"OpenProcess failed: "<<GetLastError()<<L"\n";return 1;}
 wchar_t executable[32768]{};DWORD size=32768;if(!QueryFullProcessImageNameW(process,0,executable,&size)||_wcsicmp(std::filesystem::path(executable).filename().c_str(),L"OAR-Win64-Shipping.exe")!=0){std::wcerr<<L"Target must be OAR-Win64-Shipping.exe\n";return 2;}
 Handle modules{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid)};MODULEENTRY32W loaded{sizeof(loaded)};if(Module32FirstW(modules,&loaded))do{if(_wcsicmp(loaded.szModule,path.filename().c_str())==0){std::wcout<<L"DLL already loaded.\n";return 0;}}while(Module32NextW(modules,&loaded));
 BOOL wow=false;if(!IsWow64Process(process,&wow)||wow){std::wcerr<<L"Expected a 64-bit game process\n";return 2;}
 auto wide=path.wstring();auto bytes=(wide.size()+1)*sizeof(wchar_t);void* remote=VirtualAllocEx(process,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!remote){std::wcerr<<L"VirtualAllocEx failed\n";return 1;}
 SIZE_T written=0;if(!WriteProcessMemory(process,remote,wide.c_str(),bytes,&written)||written!=bytes){VirtualFreeEx(process,remote,0,MEM_RELEASE);return 1;}
 auto load=RemoteLoader(pid);if(!load){VirtualFreeEx(process,remote,0,MEM_RELEASE);std::wcerr<<L"Cannot resolve target loader address\n";return 1;}Handle thread{CreateRemoteThread(process,nullptr,0,load,remote,0,nullptr)};if(!thread){VirtualFreeEx(process,remote,0,MEM_RELEASE);std::wcerr<<L"CreateRemoteThread failed: "<<GetLastError()<<L"\n";return 1;}
 auto wait=WaitForSingleObject(thread,15000);if(wait!=WAIT_OBJECT_0){std::wcerr<<L"Loader still running; remote path retained. Check game and logs.\n";return 1;}
 DWORD result=0;GetExitCodeThread(thread,&result);VirtualFreeEx(process,remote,0,MEM_RELEASE);if(!result){std::wcerr<<L"LoadLibrary failed in game process\n";return 1;}
 std::wcout<<L"DLL loaded.\n";return 0;
}
