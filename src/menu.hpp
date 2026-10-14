#pragma once
#include "imgui.h"
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>
namespace trace {
enum class Kind { Toggle, Slider, Choice, Key };
struct Setting {
 std::string id, label, group;
 int page, tab, column;
 Kind kind;
 float value, initial, low=0, high=1;
 std::string unit;
 std::vector<std::string> choices;
};
class Menu {
public:
 explicit Menu(std::filesystem::path profile_dir);
 void InitializeFonts(std::filesystem::path font_dir = {});
 void Render();
 std::string Translate(const std::string& text) const;
 float Get(const std::string& id) const;
 bool Set(const std::string& id, float value);
 bool Save();
 bool Load(const std::string& name);
 bool CreateProfile(const std::string& name);
 bool DeleteProfile(const std::string& name);
 bool Dirty() const;
 void Reset();
 void Navigate(int page, int tab=0);
 const std::vector<Setting>& Settings() const { return settings_; }
 void SetRuntimeCapabilities(const std::vector<std::string>& ids) { runtime_mode_=true; supported_={ids.begin(),ids.end()}; }
 void SetRuntimeStatus(std::string status) { runtime_status_=std::move(status); }
 bool visible=true;
private:
 bool runtime_mode_=false;
 std::unordered_set<std::string> supported_;
 std::string runtime_status_;
 std::vector<Setting> settings_;
 std::vector<float> saved_;
 std::filesystem::path profile_dir_;
 std::string profile_="Default", status_;
 int page_=1, tab_=0, binding_=-1;
 bool search_open_=false;
 char query_[128]{}, profile_name_[64]{};
 ImFont *regular_=nullptr, *bold_=nullptr, *title_=nullptr;
 void Row(Setting& s);
 void Profiles();
 void Snapshot();
};
}
