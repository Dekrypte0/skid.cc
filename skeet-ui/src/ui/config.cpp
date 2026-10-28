#include "config.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace cfg {

namespace {

std::map<std::string, Entry> g_entries;
std::unordered_map<std::string, std::string> g_pending;

std::string g_page = "Menu";
std::string g_sub = "Main";
int g_subIndex = 0;
std::string g_group = "General";
std::string g_dir;

std::string ExeDir()
{
    char path[MAX_PATH]{};
    const DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string p(path, n);
    const size_t slash = p.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
}

std::string Sanitize(const std::string& name)
{
    std::string out;
    for (char ch : name)
    {
        const unsigned char u = (unsigned char)ch;
        if ((u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') ||
            ch == ' ' || ch == '-' || ch == '_' || ch == '.')
            out.push_back(ch);
        else
            out.push_back('_');
    }
    while (!out.empty() && out.front() == ' ')
        out.erase(out.begin());
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    if (out.empty() || out == ".")
        out = "default";
    return out;
}

std::string PathFor(const std::string& name)
{
    return g_dir + "\\" + Sanitize(name) + ".cfg";
}

std::string Trim(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
    if (i > 0)
        s.erase(0, i);
    return s;
}

void ApplyValue(Entry& e, const char* value)
{
    if (!e.ptr || !value)
        return;

    switch (e.type)
    {
    case Type::Bool:
        *reinterpret_cast<bool*>(e.ptr) = (std::atoi(value) != 0);
        break;
    case Type::Int:
    case Type::Key:
        *reinterpret_cast<int*>(e.ptr) = std::atoi(value);
        break;
    case Type::Float:
        *reinterpret_cast<float*>(e.ptr) = (float)std::atof(value);
        break;
    case Type::Color:
    {
        float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
        if (std::sscanf(value, "%f %f %f %f", &r, &g, &b, &a) >= 3)
            *reinterpret_cast<ImVec4*>(e.ptr) = ImVec4(r, g, b, a);
        break;
    }
    case Type::String:
        strncpy_s(reinterpret_cast<char*>(e.ptr), (size_t)e.cap, value, _TRUNCATE);
        break;
    }
}

std::string ValueText(const Entry& e)
{
    if (!e.ptr)
        return std::string();

    char buf[128]{};
    switch (e.type)
    {
    case Type::Bool:
        return *reinterpret_cast<const bool*>(e.ptr) ? "1" : "0";
    case Type::Int:
    case Type::Key:
        std::snprintf(buf, sizeof(buf), "%d", *reinterpret_cast<const int*>(e.ptr));
        return buf;
    case Type::Float:
        std::snprintf(buf, sizeof(buf), "%.6f", *reinterpret_cast<const float*>(e.ptr));
        return buf;
    case Type::Color:
    {
        const ImVec4& c = *reinterpret_cast<const ImVec4*>(e.ptr);
        std::snprintf(buf, sizeof(buf), "%.4f %.4f %.4f %.4f", c.x, c.y, c.z, c.w);
        return buf;
    }
    case Type::String:
        return reinterpret_cast<const char*>(e.ptr);
    }
    return buf;
}

Entry* Bind(Type type, const char* label, void* ptr, int cap)
{
    if (!label || !ptr)
        return nullptr;

    const std::string key = g_page + "/" + g_sub + "/" + g_group + "/" + label;
    auto it = g_entries.find(key);
    if (it != g_entries.end())
    {
        if (it->second.type != type)
            return nullptr;
        Entry* e = &it->second;
        // Late load: values that were read before the widget existed.
        if (auto p = g_pending.find(key); p != g_pending.end())
        {
            ApplyValue(*e, p->second.c_str());
            g_pending.erase(p);
        }
        return e;
    }

    Entry e;
    e.key = key;
    e.label = label;
    e.page = g_page;
    e.sub = g_sub;
    e.subIndex = g_subIndex;
    e.group = g_group;
    e.type = type;
    e.ptr = ptr;
    e.cap = cap;

    switch (type)
    {
    case Type::Bool:
        e.defB = *reinterpret_cast<bool*>(ptr);
        break;
    case Type::Int:
    case Type::Key:
        e.defI = *reinterpret_cast<int*>(ptr);
        break;
    case Type::Float:
        e.defF = *reinterpret_cast<float*>(ptr);
        break;
    case Type::Color:
        e.defC = *reinterpret_cast<ImVec4*>(ptr);
        break;
    case Type::String:
        e.defS = reinterpret_cast<char*>(ptr);
        break;
    }

    auto res = g_entries.emplace(key, std::move(e));
    Entry* stored = &res.first->second;

    if (auto p = g_pending.find(key); p != g_pending.end())
    {
        ApplyValue(*stored, p->second.c_str());
        g_pending.erase(p);
    }
    return stored;
}

} // namespace

void Init()
{
    g_dir = ExeDir() + "\\configs";
    std::error_code ec;
    std::filesystem::create_directories(g_dir, ec);

    // Auto-load the default profile if it exists; entries bind later and pull
    // their values from the pending map.
    if (std::filesystem::exists(PathFor("default")))
        Load("default");
}

void SetPage(const char* page, int subIndex, const char* sub)
{
    g_page = page ? page : "Menu";
    g_subIndex = subIndex;
    g_sub = sub ? sub : "Main";
}

void SetGroup(const char* group)
{
    g_group = group ? group : "General";
}

const std::string& Dir()
{
    return g_dir;
}

size_t Count()
{
    return g_entries.size();
}

const std::map<std::string, Entry>& Entries()
{
    return g_entries;
}

Entry* BindBool(const char* label, bool& v) { return Bind(Type::Bool, label, &v, 0); }
Entry* BindInt(const char* label, int& v) { return Bind(Type::Int, label, &v, 0); }
Entry* BindFloat(const char* label, float& v) { return Bind(Type::Float, label, &v, 0); }
Entry* BindColor(const char* label, ImVec4& v) { return Bind(Type::Color, label, &v, 0); }
Entry* BindString(const char* label, char* buf, int cap) { return Bind(Type::String, label, buf, cap); }
Entry* BindKey(const char* label, int& v) { return Bind(Type::Key, label, &v, 0); }

bool Save(const std::string& name)
{
    if (g_dir.empty())
        return false;

    const std::string path = PathFor(name);
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f)
        return false;

    std::fprintf(f, "# skeet-ui config\n# key=value\n");
    for (const auto& [key, entry] : g_entries)
    {
        const std::string value = ValueText(entry);
        std::fprintf(f, "%s=%s\n", key.c_str(), value.c_str());
    }
    std::fclose(f);
    return true;
}

bool Load(const std::string& name)
{
    const std::string path = PathFor(name);
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f)
        return false;

    char line[512];
    while (std::fgets(line, sizeof(line), f))
    {
        std::string s = Trim(line);
        if (s.empty() || s[0] == '#')
            continue;

        const size_t eq = s.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = s.substr(0, eq);
        std::string value = s.substr(eq + 1);

        if (auto it = g_entries.find(key); it != g_entries.end())
            ApplyValue(it->second, value.c_str());
        else
            g_pending[key] = value;
    }
    std::fclose(f);
    return true;
}

bool Delete(const std::string& name)
{
    const std::string path = PathFor(name);
    return DeleteFileA(path.c_str()) != 0;
}

std::vector<std::string> List()
{
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& it : std::filesystem::directory_iterator(g_dir, ec))
    {
        if (!it.is_regular_file(ec))
            continue;
        if (it.path().extension() == ".cfg")
            out.push_back(it.path().stem().string());
    }
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        return _stricmp(a.c_str(), b.c_str()) < 0;
    });
    return out;
}

} // namespace cfg
