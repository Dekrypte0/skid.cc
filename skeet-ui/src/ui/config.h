#pragma once

#include "imgui.h"

#include <map>
#include <string>
#include <vector>

// Tiny value registry behind every widget. Widgets bind their value once and the
// registry handles defaults, serialization and the search index.
namespace cfg {

enum class Type { Bool, Int, Float, Color, String, Key };

struct Entry {
    std::string key;
    std::string label;
    std::string page;
    std::string sub;
    std::string group;
    int subIndex = 0;

    Type type = Type::Bool;
    void* ptr = nullptr;
    int cap = 0;

    bool defB = false;
    int defI = 0;
    float defF = 0.0f;
    ImVec4 defC = ImVec4(0, 0, 0, 1);
    std::string defS;
};

void Init();
void SetPage(const char* page, int subIndex, const char* sub);
void SetGroup(const char* group);

const std::string& Dir();
size_t Count();
const std::map<std::string, Entry>& Entries();

Entry* BindBool(const char* label, bool& v);
Entry* BindInt(const char* label, int& v);
Entry* BindFloat(const char* label, float& v);
Entry* BindColor(const char* label, ImVec4& v);
Entry* BindString(const char* label, char* buf, int cap);
Entry* BindKey(const char* label, int& v);

bool Save(const std::string& name);
bool Load(const std::string& name);
bool Delete(const std::string& name);
std::vector<std::string> List();

} // namespace cfg
