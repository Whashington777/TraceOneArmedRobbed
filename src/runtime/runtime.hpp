#pragma once
#include <map>
#include <string>
#include <vector>
namespace trace::runtime {
using Config=std::map<std::string,float>;
std::vector<std::string> Supported();
void SetConfig(const Config& config);
void RequestRestore();
void SetMenuVisible(bool visible);
std::string Status();
void Tick();
void PollAttach();
void InitializePath(void* module);
}
