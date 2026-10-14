#include "runtime.hpp"
#include "menu.hpp"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <MinHook.h>
#include <d3d11.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
using Present=HRESULT(__stdcall*)(IDXGISwapChain*,UINT,UINT);
using Resize=HRESULT(__stdcall*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
Present original_present{};Resize original_resize{};void* present_target{};void* resize_target{};
ID3D11Device* device{};ID3D11DeviceContext* context{};ID3D11RenderTargetView* target{};HWND window{};WNDPROC original_wndproc{};
std::unique_ptr<trace::Menu> menu;std::filesystem::path mod_dir;std::recursive_mutex ui_mutex;bool initialized=false;std::atomic<bool> panic{false};
void Log(const char* text){std::ofstream(mod_dir/"overlay.log",std::ios::app)<<GetTickCount64()<<" "<<text<<'\n';}
void CreateTarget(IDXGISwapChain* swap){ID3D11Texture2D* back{};if(SUCCEEDED(swap->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&back)))){device->CreateRenderTargetView(back,nullptr,&target);back->Release();}}
LRESULT CALLBACK WindowProc(HWND h,UINT msg,WPARAM w,LPARAM l){std::lock_guard<std::recursive_mutex> guard(ui_mutex);if(initialized){ImGui_ImplWin32_WndProcHandler(h,msg,w,l);if(msg==WM_KEYDOWN&&w==VK_F10)panic=true;if(menu->visible){auto& io=ImGui::GetIO();bool mouse=(msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST)||msg==WM_INPUT;bool keyboard=msg>=WM_KEYFIRST&&msg<=WM_KEYLAST;if((mouse&&io.WantCaptureMouse)||(keyboard&&io.WantCaptureKeyboard))return 1;}}return CallWindowProcW(original_wndproc,h,msg,w,l);}
HRESULT __stdcall PresentHook(IDXGISwapChain* swap,UINT interval,UINT flags){
 {std::lock_guard<std::recursive_mutex> guard(ui_mutex);
 if(!initialized&&SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device),reinterpret_cast<void**>(&device)))){
  DXGI_SWAP_CHAIN_DESC desc{};swap->GetDesc(&desc);window=desc.OutputWindow;device->GetImmediateContext(&context);CreateTarget(swap);ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
  menu=std::make_unique<trace::Menu>(mod_dir/"profiles");menu->InitializeFonts(mod_dir/"fonts");menu->SetRuntimeCapabilities(trace::runtime::Supported());
  ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(device,context);original_wndproc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(WindowProc)));initialized=true;Log("ImGui DX11 initialized; INSERT menu, F10 reset");
 }
 trace::runtime::PollAttach();
 if(initialized&&target){ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();if(panic.exchange(false)){menu->Reset();trace::runtime::RequestRestore();}
  menu->SetRuntimeStatus(trace::runtime::Status());menu->Render();ImGui::GetIO().MouseDrawCursor=menu->visible;trace::runtime::SetMenuVisible(menu->visible);static std::vector<float> previous_values;bool changed=previous_values.size()!=menu->Settings().size();if(!changed)for(size_t i=0;i<previous_values.size();++i)if(previous_values[i]!=menu->Settings()[i].value){changed=true;break;}
  if(changed){trace::runtime::Config config;previous_values.clear();previous_values.reserve(menu->Settings().size());for(auto& setting:menu->Settings()){config[setting.id]=setting.value;previous_values.push_back(setting.value);}trace::runtime::SetConfig(config);}ImGui::Render();
   
  if(ImGui::GetDrawData()->TotalVtxCount>0){ID3D11RenderTargetView* previous{};ID3D11DepthStencilView* depth{};context->OMGetRenderTargets(1,&previous,&depth);context->OMSetRenderTargets(1,&target,nullptr);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());context->OMSetRenderTargets(1,&previous,depth);if(previous)previous->Release();if(depth)depth->Release();}
 }
 }return original_present(swap,interval,flags);
}
HRESULT __stdcall ResizeHook(IDXGISwapChain* swap,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags){std::lock_guard<std::recursive_mutex> guard(ui_mutex);if(initialized){ImGui_ImplDX11_InvalidateDeviceObjects();if(target){target->Release();target=nullptr;}}auto result=original_resize(swap,count,width,height,format,flags);if(initialized&&SUCCEEDED(result)){CreateTarget(swap);ImGui_ImplDX11_CreateDeviceObjects();}return result;}
DWORD WINAPI Start(void* module){wchar_t path[32768]{};GetModuleFileNameW(static_cast<HMODULE>(module),path,32768);mod_dir=std::filesystem::path(path).parent_path();trace::runtime::InitializePath(module);
 WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=static_cast<HINSTANCE>(module);wc.lpszClassName=L"TraceD3DProbe";RegisterClassExW(&wc);auto probe=CreateWindowExW(0,wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
 DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=1;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=probe;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 IDXGISwapChain* swap{};ID3D11Device* dummy_device{};ID3D11DeviceContext* dummy_context{};
 auto result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&dummy_device,nullptr,&dummy_context);
 if(SUCCEEDED(result)){auto table=*reinterpret_cast<void***>(swap);present_target=table[8];resize_target=table[13];if(MH_Initialize()==MH_OK&&MH_CreateHook(present_target,reinterpret_cast<void*>(PresentHook),reinterpret_cast<void**>(&original_present))==MH_OK&&MH_CreateHook(resize_target,reinterpret_cast<void*>(ResizeHook),reinterpret_cast<void**>(&original_resize))==MH_OK&&MH_EnableHook(MH_ALL_HOOKS)==MH_OK)Log("DX11 hooks installed");else Log("DX11 hook installation failed");}
 else Log("D3D11 probe failed: use DX11 renderer");
 if(dummy_context)dummy_context->Release();if(dummy_device)dummy_device->Release();if(swap)swap->Release();if(probe)DestroyWindow(probe);UnregisterClassW(wc.lpszClassName,wc.hInstance);return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);auto thread=CreateThread(nullptr,0,Start,module,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
