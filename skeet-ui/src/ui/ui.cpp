#include "ui.h"
#include "config.h"
#include "icons.h"
#include "theme.h"

#include "imgui_internal.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace ui {
namespace {

using theme::Px;

// ---------------------------------------------------------------------------
// constants
// ---------------------------------------------------------------------------
constexpr float kW = 780.0f;
constexpr float kH = 620.0f;
constexpr float kRailW = 76.0f;
constexpr float kHeaderH = 46.0f;
constexpr float kFooterH = 28.0f;
constexpr float kPageHeadH = 46.0f;
constexpr float kPad = 14.0f;
constexpr float kColGap = 12.0f;

struct TabDef {
    const char* name;
    icons::Id icon;
    int subCount;
    const char* subs[3];
};

const TabDef kTabs[] = {
    { "Aimbot",  icons::Id::Rage,     3, { "Aimbot", "Anti-Aim", "Triggerbot" } },
    { "Visuals", icons::Id::Visuals,  3, { "Players", "World", "Colors" } },
    { "Misc",    icons::Id::Misc,     3, { "Main", "Movement", "View" } },
    { "Skins",   icons::Id::Skins,    2, { "Weapons", "Gloves" } },
    { "Players", icons::Id::Players,  1, { "Overview" } },
    { "Config",  icons::Id::Config,   2, { "Configs", "Settings" } },
};
constexpr int kTabCount = (int)(sizeof(kTabs) / sizeof(kTabs[0]));

static const char* const kHitbox[] = { "Head", "Neck", "Chest", "Stomach", "Nearest" };
static const char* const kPriority[] = { "FOV", "Distance", "Health" };
static const char* const kPitch[] = { "Off", "Down", "Up", "Fake" };
static const char* const kYaw[] = { "Off", "Forward", "Backward", "Left", "Right" };
static const char* const kLegs[] = { "Off", "Static", "Jitter" };
static const char* const kTrigMode[] = { "Hold", "Toggle" };
static const char* const kHitgroup[] = { "All", "Head", "Chest", "Stomach" };
static const char* const kBoxStyle[] = { "Full", "Corner", "Outline" };
static const char* const kNadeStyle[] = { "Text", "Icon", "Both" };
static const char* const kWeapons[] = { "AK-47", "M4A4", "M4A1-S", "AWP", "Desert Eagle", "USP-S", "Glock-18", "Karambit" };
static const char* const kSkins[] = { "Default", "Asiimov", "Neon Rider", "Printstream", "Fade", "Doppler", "Hyper Beast", "Vulcan" };
static const char* const kGloves[] = { "Default", "Sport", "Driver", "Hand Wraps", "Moto", "Specialist" };
static const char* const kGloveSkins[] = { "Default", "Amphibious", "King Snake", "Superconductor", "Fade" };

// ---------------------------------------------------------------------------
// settings (demo values; every widget is bound to the config registry)
// ---------------------------------------------------------------------------
struct Settings {
    // aimbot
    bool aimEnable = true;
    int aimKey = 0;
    float aimFov = 35.0f;
    float aimSmooth = 4.0f;
    int aimHitbox = 0;
    bool aimVisible = true;
    bool aimWall = false;
    bool aimAutoFire = false;
    bool aimAutoScope = true;
    bool aimBacktrack = true;
    float aimBacktrackMs = 200.0f;
    bool aimResolver = true;
    float aimMinDmg = 30.0f;
    float aimHeadScale = 65.0f;
    int aimPriority = 0;
    bool aimMulti = true;
    float aimMpScale = 45.0f;
    bool aimTeam = true;
    bool aimPredict = true;
    float aimReact = 120.0f;
    bool aimHumanize = true;
    float aimCurve = 45.0f;
    bool aimMicro = false;
    float aimJitter = 0.8f;
    bool aimRand = true;

    // anti-aim
    bool aaEnable = false;
    int aaPitch = 0;
    int aaYaw = 1;
    float aaJitter = 0.0f;
    int aaKey = 0;
    bool aaFakeLag = false;
    bool aaDesync = true;
    float aaDesyncAmt = 35.0f;
    bool aaRoll = false;
    float aaRollAmt = 20.0f;
    bool aaHideShots = false;
    int aaLegs = 0;
    bool aaManual = false;
    int aaLeftKey = 0;
    int aaRightKey = 0;
    int aaBackKey = 0;
    bool aaFakeDuck = false;
    float aaDuckAmt = 60.0f;
    bool aaSlide = false;
    float aaBodyYaw = 60.0f;
    float aaYawOff = 0.0f;

    // trigger
    bool trEnable = false;
    int trKey = 0;
    int trMode = 0;
    float trDelay = 120.0f;
    float trHitchance = 55.0f;
    float trFov = 2.5f;
    int trHitgroup = 0;
    bool trCrosshair = true;
    bool trScoped = false;
    bool trSmoke = false;
    bool trFlash = false;
    bool trAutoStop = false;
    float trStopSpeed = 60.0f;

    // visuals
    bool espEnable = true;
    bool espBox = true;
    bool espName = true;
    bool espHealth = true;
    bool espWeapon = true;
    bool espDist = true;
    int espBoxStyle = 1;
    bool espSkeleton = false;
    bool espArmor = false;
    bool espAmmo = false;
    bool espFlags = true;
    bool espTeam = false;
    bool espVisColor = true;
    float espMaxDist = 1200.0f;
    bool espArrows = false;
    float espFade = 2.5f;

    bool wWeapons = true;
    bool wC4 = true;
    bool wNades = true;
    bool wBombTimer = true;
    bool wDefuse = true;
    int wNadeStyle = 2;
    bool wGlow = false;
    bool wTracers = true;
    bool wImpacts = false;
    bool wHitmarker = true;
    bool wDamage = true;
    float wFade = 2.5f;
    bool wNoSmoke = false;
    bool wNoFlash = false;
    bool wNoDecals = false;

    ImVec4 cEnemy = ImVec4(0.94f, 0.35f, 0.32f, 1.0f);
    ImVec4 cTeam = ImVec4(0.36f, 0.72f, 0.95f, 1.0f);
    ImVec4 cBox = ImVec4(0.58f, 0.83f, 0.33f, 1.0f);
    ImVec4 cName = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    ImVec4 cSkel = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    ImVec4 cHealth = ImVec4(0.45f, 0.85f, 0.35f, 1.0f);
    ImVec4 cTracer = ImVec4(0.58f, 0.83f, 0.33f, 1.0f);
    ImVec4 cBomb = ImVec4(1.0f, 0.66f, 0.25f, 1.0f);
    ImVec4 cGrenade = ImVec4(0.56f, 0.78f, 1.0f, 1.0f);

    // misc
    bool mBhop = true;
    bool mStrafe = true;
    bool mAccept = true;
    bool mWatermark = true;
    bool mSpecList = false;
    bool mPanic = false;
    int mPanicKey = 0;
    bool mAutoPistol = false;
    bool mAutoReload = true;
    bool mClantag = false;
    char mClan[32] = "skeet ui";
    bool mChatSpam = false;
    char mSpam[64] = "skeet ui on top";
    bool mNameChange = false;
    char mName[32] = "skibidi";

    // movement
    bool mvAutoPeek = false;
    int mvAutoPeekKey = 0;
    bool mvLadder = true;
    bool mvEdge = true;
    bool mvJumpBug = false;
    float mvJumpChance = 70.0f;
    bool mvAutoJump = false;
    bool mvAirStrafe = true;
    float mvStrafeSpeed = 85.0f;
    bool mvSlowWalk = false;
    int mvSlowKey = 0;
    float mvWalkSpeed = 45.0f;
    float mvBhopChance = 90.0f;

    // view
    bool vThird = false;
    int vThirdKey = 0;
    float vCamDist = 120.0f;
    bool vFov = false;
    float vFovVal = 90.0f;
    bool vVmFov = false;
    float vVmFovVal = 68.0f;
    bool vNoScope = false;
    bool vNoZoom = false;
    bool vAspect = true;
    float vAspectVal = 1.33f;
    bool vNoBlur = false;
    bool vFlashReduce = false;
    bool vNoPunch = false;

    // skins
    int skWeapon = 0;
    int skSkin = 1;
    float skWear = 12.0f;
    int skSeed = 420;
    char skStat[8] = "1337";
    bool skStatTrak = true;
    bool skAutoApply = false;
    bool skEquip = true;
    bool glEnable = false;
    int glType = 1;
    int glSkin = 2;
    float glWear = 8.0f;
    bool glReplace = true;
    bool glTeam = false;

    // players page
    char plSpoof[32] = "skibidi";
    bool plReveal = true;
    bool plShowBots = false;

    // menu
    bool sAnim = true;
    bool sTips = true;
    bool sShowFps = true;
    ImVec4 sAccent = ImVec4(149.0f / 255.0f, 212.0f / 255.0f, 84.0f / 255.0f, 1.0f);
} gs;

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------
bool g_visible = true;
bool g_close = false;
ImVec2 g_drag(0.0f, 0.0f);

int g_tab = 0;
int g_sub[kTabCount] = { 0, 0, 0, 0, 0, 0 };

std::string g_highlightKey;
float g_highlightT = 0.0f;

struct ToastItem {
    std::string title;
    std::string body;
    bool good = true;
    float t = 0.0f;
};
std::vector<ToastItem> g_toasts;

bool g_paletteOpen = false;
char g_search[96] = "";
bool g_focusSearch = false;

char g_cfgName[48] = "default";
std::vector<std::string> g_cfgList;
float g_cfgRefresh = 0.0f;

int* g_captureKey = nullptr;
int g_playerSel = 0;

ImVec2 g_O(0.0f, 0.0f);
char g_user[64] = "user";

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------
inline ImU32 C(unsigned rgb, int a = 255) { return theme::Hex(rgb, a); }

float TW(ImFont* font, float size, const char* text)
{
    if (!font || !text)
        return 0.0f;
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

void ToUpper(const char* src, char* dst, size_t cap)
{
    if (!src || !dst || cap == 0)
        return;
    size_t i = 0;
    for (; src[i] && i + 1 < cap; ++i)
        dst[i] = (char)std::toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

float TrackedWidth(ImFont* font, float size, const char* text, float tracking)
{
    if (!text || !*text)
        return 0.0f;
    float w = 0.0f;
    for (const char* p = text; *p; ++p)
    {
        char ch[2] = { *p, 0 };
        w += TW(font, size, ch) + tracking;
    }
    return w - tracking;
}

void TrackedText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 col, const char* text, float tracking)
{
    if (!dl || !text)
        return;
    float x = pos.x;
    for (const char* p = text; *p; ++p)
    {
        char ch[2] = { *p, 0 };
        dl->AddText(font, size, ImVec2(x, pos.y), col, ch);
        x += TW(font, size, ch) + tracking;
    }
}

bool SameVec4(const ImVec4& a, const ImVec4& b)
{
    return std::fabs(a.x - b.x) < 1e-4f && std::fabs(a.y - b.y) < 1e-4f &&
           std::fabs(a.z - b.z) < 1e-4f && std::fabs(a.w - b.w) < 1e-4f;
}

const char* KeyName(int key)
{
    switch (key)
    {
    case 0: return "none";
    case ImGuiKey_MouseRight: return "M2";
    case ImGuiKey_MouseMiddle: return "M3";
    case ImGuiKey_MouseX1: return "M4";
    case ImGuiKey_MouseX2: return "M5";
    default:
    {
        const char* n = ImGui::GetKeyName((ImGuiKey)key);
        return (n && *n) ? n : "?";
    }
    }
}

float AnimTo(float target, float speed)
{
    if (!gs.sAnim)
        return target;
    const ImGuiID id = ImGui::GetID("##anim");
    float* v = ImGui::GetStateStorage()->GetFloatRef(id, target);
    const float k = ImMin(1.0f, ImGui::GetIO().DeltaTime * speed);
    *v += (target - *v) * k;
    return *v;
}

void Tip(const char* text)
{
    if (!gs.sTips || !text || !*text)
        return;
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(8), Px(6)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(3));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, C(0x1A1A1A, 250));
    ImGui::PushStyleColor(ImGuiCol_Border, C(0x323232));
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(text);
    ImGui::EndTooltip();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void DrawHighlight(cfg::Entry* entry, ImVec2 a, ImVec2 b)
{
    if (!entry || g_highlightT <= 0.0f || g_highlightKey != entry->key)
        return;
    const float k = ImMin(1.0f, g_highlightT);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pad(Px(2), Px(1));
    dl->AddRectFilled(a - pad, b + pad, theme::Acc(0.6f, (int)(24.0f * k)), Px(2));
    dl->AddRect(a - pad, b + pad, theme::Acc(1.0f, (int)(210.0f * k)), Px(2), 0, Px(1.5f));
}

// ---------------------------------------------------------------------------
// widgets
// ---------------------------------------------------------------------------
bool CheckboxRow(const char* label, bool& value, const char* tooltip = nullptr)
{
    ImGui::PushID(label);
    cfg::Entry* entry = cfg::BindBool(label, value);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = Px(24);
    ImGui::InvisibleButton("##row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    if (ImGui::IsItemClicked(0))
        value = !value;
    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    Tip(tooltip);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float t = AnimTo(value ? 1.0f : 0.0f, 18.0f);
    const float bs = Px(11);
    const ImVec2 ba(p.x, p.y + (h - bs) * 0.5f);
    const ImVec2 bb(ba.x + bs, ba.y + bs);

    dl->AddRectFilled(ba, bb, hovered ? C(0x1E1E1E) : C(0x141414), Px(2));
    if (t > 0.01f)
        dl->AddRectFilled(ba, bb, theme::Acc(1.0f, (int)(230.0f * t)), Px(2));
    dl->AddRect(ba, bb, t > 0.5f ? theme::Acc(0.85f) : C(hovered ? 0x3A3A3A : theme::col::Border),
                Px(2), 0, Px(1));
    if (t > 0.35f)
        icons::Draw(dl, icons::Id::Check, ImVec2((ba.x + bb.x) * 0.5f, (ba.y + bb.y) * 0.5f),
                    bs * 0.78f, IM_COL32(8, 8, 8, (int)(235.0f * t)), Px(1.6f));

    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(bb.x + Px(8), p.y + (h - theme::SizeMain) * 0.5f),
                hovered ? C(theme::col::TextBright) : C(theme::col::Text), label);

    DrawHighlight(entry, a, b);
    ImGui::PopID();
    return entry != nullptr;
}

template <typename T>
bool SliderCore(const char* label, cfg::Entry* entry, T& value, T vmin, T vmax,
                const char* fmt, const char* tooltip)
{
    ImGui::PushID(label);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = Px(24);
    ImGui::InvisibleButton("##row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();

    value = ImClamp(value, vmin, vmax);

    if (active && w > 1.0f)
    {
        const float t = ImClamp((ImGui::GetIO().MousePos.x - (p.x + Px(1))) / (w - Px(2)), 0.0f, 1.0f);
        const double nv = (double)vmin + (double)t * ((double)vmax - (double)vmin);
        if constexpr (std::is_integral<T>::value)
            value = (T)std::llround(nv);
        else
            value = (T)nv;
    }
    if (hovered && !active)
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            if constexpr (std::is_integral<T>::value)
                value = ImClamp((T)(value + (T)wheel), vmin, vmax);
            else
                value = ImClamp((T)(value + wheel * ((vmax - vmin) / 100.0f)), vmin, vmax);
        }
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        if constexpr (std::is_integral<T>::value)
            value = entry ? (T)entry->defI : vmin;
        else
            value = entry ? (T)entry->defF : vmin;
    }

    if (hovered || active)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    Tip(tooltip);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float t = (float)(((double)value - (double)vmin) / ((double)vmax - (double)vmin));
    const float ty = p.y + h - Px(3);
    dl->AddRectFilled(ImVec2(p.x, ty), ImVec2(p.x + w, ty + Px(2)), C(theme::col::Track), Px(1));
    dl->AddRectFilled(ImVec2(p.x, ty), ImVec2(p.x + w * ImClamp(t, 0.0f, 1.0f), ty + Px(2)),
                      theme::Acc(1.0f, (hovered || active) ? 255 : 200), Px(1));

    char val[64];
    if constexpr (std::is_integral<T>::value)
        std::snprintf(val, sizeof(val), fmt, (int)value);
    else
        std::snprintf(val, sizeof(val), fmt, (double)value);

    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(p.x, p.y + (h - theme::SizeMain) * 0.5f),
                (hovered || active) ? C(theme::col::TextBright) : C(theme::col::Text), label);
    const float vw = TW(theme::FontMain, theme::SizeMain, val);
    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(p.x + w - vw, p.y + (h - theme::SizeMain) * 0.5f),
                theme::Acc(1.0f, (hovered || active) ? 255 : 190), val);

    DrawHighlight(entry, a, b);
    ImGui::PopID();
    return entry != nullptr;
}

bool SliderRowF(const char* label, float& value, float vmin, float vmax, const char* fmt,
                const char* tooltip = nullptr)
{
    cfg::Entry* e = cfg::BindFloat(label, value);
    return SliderCore(label, e, value, vmin, vmax, fmt, tooltip);
}

bool SliderRowI(const char* label, int& value, int vmin, int vmax, const char* fmt,
                const char* tooltip = nullptr)
{
    cfg::Entry* e = cfg::BindInt(label, value);
    return SliderCore(label, e, value, vmin, vmax, fmt, tooltip);
}

bool DropdownRow(const char* label, int& value, const char* const* items, int count,
                 const char* tooltip = nullptr)
{
    ImGui::PushID(label);
    cfg::Entry* entry = cfg::BindInt(label, value);
    value = ImClamp(value, 0, count - 1);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = Px(24);
    ImGui::InvisibleButton("##row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked(0))
        ImGui::OpenPopup("##dd");

    Tip(tooltip);

    const float boxW = Px(120);
    const ImVec2 ba(p.x + w - boxW, p.y + Px(2));
    const ImVec2 bb(p.x + w, p.y + h - Px(2));
    const bool open = ImGui::IsPopupOpen("##dd");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ba, bb, (hovered || open) ? C(0x232323) : C(0x1A1A1A), Px(2));
    dl->AddRect(ba, bb, (hovered || open) ? C(0x383838) : C(0x262626), Px(2), 0, Px(1));

    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(p.x, p.y + (h - theme::SizeMain) * 0.5f),
                hovered ? C(theme::col::TextBright) : C(theme::col::Text), label);

    const char* current = items[value];
    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(ba.x + Px(8), p.y + (h - theme::SizeMain) * 0.5f),
                (hovered || open) ? C(theme::col::TextBright) : C(0xB8B8B8), current);
    icons::Draw(dl, icons::Id::ChevronDown,
                ImVec2(bb.x - Px(11), (ba.y + bb.y) * 0.5f), Px(8),
                (hovered || open) ? C(0xD8D8D8) : C(theme::col::Dim), Px(1.4f));

    ImGui::SetNextWindowPos(ImVec2(ba.x, bb.y + Px(2)));
    ImGui::SetNextWindowSizeConstraints(ImVec2(boxW, 0.0f), ImVec2(boxW, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(4), Px(4)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (ImGui::BeginPopup("##dd"))
    {
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        for (int i = 0; i < count; ++i)
        {
            ImGui::PushID(i);
            const ImVec2 rp = ImGui::GetCursorScreenPos();
            const float rw = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("##opt", ImVec2(rw, Px(20)));
            const bool oh = ImGui::IsItemHovered();
            if (oh)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            const bool sel = (i == value);
            if (sel || oh)
            {
                pdl->AddRectFilled(rp, ImVec2(rp.x + rw, rp.y + Px(20)),
                                   sel ? theme::Acc(0.6f, 34) : C(0x242424), Px(2));
            }
            pdl->AddText(theme::FontMain, theme::SizeMain,
                         ImVec2(rp.x + Px(6), rp.y + (Px(20) - theme::SizeMain) * 0.5f),
                         sel ? theme::Acc() : (oh ? C(theme::col::TextBright) : C(theme::col::Text)),
                         items[i]);
            if (sel)
                icons::Draw(pdl, icons::Id::Check,
                            ImVec2(rp.x + rw - Px(10), rp.y + Px(10)), Px(9), theme::Acc(), Px(1.4f));
            if (ImGui::IsItemClicked(0))
            {
                value = i;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);

    DrawHighlight(entry, a, b);
    ImGui::PopID();
    return entry != nullptr;
}

bool KeyRow(const char* label, int& key, const char* tooltip = nullptr)
{
    ImGui::PushID(label);
    cfg::Entry* entry = cfg::BindKey(label, key);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = Px(24);
    ImGui::InvisibleButton("##row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    const bool capturing = (g_captureKey == &key);

    if (!capturing)
    {
        if (ImGui::IsItemClicked(0))
            g_captureKey = &key;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            key = 0;
    }
    else
    {
        if (ImGui::IsMouseClicked(0) && !hovered)
        {
            g_captureKey = nullptr;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            g_captureKey = nullptr;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) ||
                 ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
        {
            key = 0;
            g_captureKey = nullptr;
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            g_captureKey = nullptr;
        }
        else if (ImGui::IsMouseClicked(3) || ImGui::IsMouseClicked(4))
        {
            key = ImGui::IsMouseClicked(3) ? ImGuiKey_MouseX1 : ImGuiKey_MouseX2;
            g_captureKey = nullptr;
        }
        else
        {
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
            {
                if (k == ImGuiKey_Escape || k == ImGuiKey_Delete || k == ImGuiKey_Backspace)
                    continue;
                if (k == ImGuiKey_MouseLeft)
                    continue;
                if (k >= ImGuiKey_MouseLeft && k <= ImGuiKey_MouseWheelY)
                {
                    if (k == ImGuiKey_MouseMiddle && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                    {
                        key = k;
                        g_captureKey = nullptr;
                        break;
                    }
                    continue;
                }
                if (ImGui::IsKeyPressed((ImGuiKey)k, false))
                {
                    key = k;
                    g_captureKey = nullptr;
                    break;
                }
            }
        }
    }

    Tip(tooltip);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float boxW = Px(66);
    const ImVec2 ba(p.x + w - boxW, p.y + Px(2));
    const ImVec2 bb(p.x + w, p.y + h - Px(2));
    dl->AddRectFilled(ba, bb, capturing ? theme::Acc(0.3f, 30) : (hovered ? C(0x232323) : C(0x1A1A1A)),
                      Px(2));
    dl->AddRect(ba, bb, capturing ? theme::Acc(1.0f) : C(hovered ? 0x383838 : 0x262626), Px(2), 0, Px(1));

    const char* text = capturing ? "press key" : KeyName(key);
    const float tw = TW(theme::FontMain, theme::SizeMain, text);
    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2((ba.x + bb.x) * 0.5f - tw * 0.5f, p.y + (h - theme::SizeMain) * 0.5f),
                capturing ? theme::Acc() : C(0xB8B8B8), text);

    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(p.x, p.y + (h - theme::SizeMain) * 0.5f),
                hovered ? C(theme::col::TextBright) : C(theme::col::Text), label);

    DrawHighlight(entry, a, b);
    ImGui::PopID();
    return entry != nullptr;
}

bool ColorRow(const char* label, ImVec4& color, const char* tooltip = nullptr)
{
    ImGui::PushID(label);
    cfg::Entry* entry = cfg::BindColor(label, color);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = Px(24);
    ImGui::InvisibleButton("##row", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked(0))
        ImGui::OpenPopup("##cp");
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && entry)
        color = entry->defC;

    Tip(tooltip);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float sw = Px(32);
    const float sh = Px(12);
    const ImVec2 sa(p.x + w - sw, p.y + (h - sh) * 0.5f);
    const ImVec2 sb(sa.x + sw, sa.y + sh);
    dl->AddRectFilled(sa, sb, ImGui::ColorConvertFloat4ToU32(color), Px(2));
    dl->AddRect(sa, sb, C(hovered ? 0x4A4A4A : 0x323232), Px(2), 0, Px(1));

    char hex[16];
    std::snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                  (int)ImClamp(color.x * 255.0f, 0.0f, 255.0f),
                  (int)ImClamp(color.y * 255.0f, 0.0f, 255.0f),
                  (int)ImClamp(color.z * 255.0f, 0.0f, 255.0f));
    const float hw = TW(theme::FontMain, theme::SizeMain, hex);
    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(sa.x - Px(8) - hw, p.y + (h - theme::SizeMain) * 0.5f),
                hovered ? C(0xA8A8A8) : C(theme::col::Dim), hex);

    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(p.x, p.y + (h - theme::SizeMain) * 0.5f),
                hovered ? C(theme::col::TextBright) : C(theme::col::Text), label);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(8), Px(8)));
    if (ImGui::BeginPopup("##cp"))
    {
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        ImGui::ColorPicker4("##picker", (float*)&color,
                            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueBar);
        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Px(4), Px(4)));
        static const ImVec4 quick[6] = {
            ImVec4(149 / 255.f, 212 / 255.f, 84 / 255.f, 1.f),
            ImVec4(86 / 255.f, 168 / 255.f, 1.f, 1.f),
            ImVec4(214 / 255.f, 92 / 255.f, 214 / 255.f, 1.f),
            ImVec4(1.f, 138 / 255.f, 72 / 255.f, 1.f),
            ImVec4(0.94f, 0.35f, 0.32f, 1.f),
            ImVec4(0.85f, 0.85f, 0.85f, 1.f),
        };
        for (int i = 0; i < 6; ++i)
        {
            ImGui::PushID(i);
            if (i > 0)
                ImGui::SameLine(0.0f, Px(4));
            const ImVec2 qp = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##q", ImVec2(Px(20), Px(16)));
            const bool qh = ImGui::IsItemHovered();
            const ImVec2 qb(qp.x + Px(20), qp.y + Px(16));
            pdl->AddRectFilled(qp, qb, ImGui::ColorConvertFloat4ToU32(quick[i]), Px(2));
            pdl->AddRect(qp, qb, qh ? C(0xDDDDDD) : C(0x3A3A3A), Px(2), 0, Px(1));
            if (ImGui::IsItemClicked(0))
            {
                const float alpha = color.w;
                color = quick[i];
                color.w = alpha;
            }
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        if (ImGui::Button("Reset", ImVec2(-1, 0)) && entry)
            color = entry->defC;
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    DrawHighlight(entry, a, b);
    ImGui::PopID();
    return entry != nullptr;
}

bool InputRow(const char* label, char* buf, int cap, const char* tooltip = nullptr)
{
    ImGui::PushID(label);
    cfg::Entry* entry = cfg::BindString(label, buf, cap);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = Px(26);
    const float fieldW = Px(142);
    const float frameH = theme::SizeMain + ImGui::GetStyle().FramePadding.y * 2.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(theme::FontMain, theme::SizeMain,
                ImVec2(p.x, p.y + (h - theme::SizeMain) * 0.5f), C(theme::col::Text), label);

    ImGui::SetCursorScreenPos(ImVec2(p.x + w - fieldW, p.y + (h - frameH) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, C(0x1A1A1A));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, C(0x222222));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, C(0x262626));
    ImGui::PushStyleColor(ImGuiCol_Border, C(0x2A2A2A));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Px(6), Px(4)));
    ImGui::SetNextItemWidth(fieldW);
    const bool changed = ImGui::InputText("##v", buf, (size_t)cap);
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    Tip(tooltip);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + ImGui::GetStyle().ItemSpacing.y));

    if (entry)
    {
        const ImVec2 a(p.x, p.y);
        const ImVec2 b(p.x + w, p.y + h);
        DrawHighlight(entry, a, b);
    }
    ImGui::PopID();
    return changed;
}

bool ButtonRow(const char* label, bool primary = false, float width = 0.0f)
{
    const float w = width > 0.0f ? Px(width) : ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Px(2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, Px(1));
    ImGui::PushStyleColor(ImGuiCol_Button, primary ? theme::Acc(1.0f) : C(0x1C1C1C));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, primary ? theme::Acc(1.08f) : C(0x242424));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, primary ? theme::Acc(0.9f) : C(0x2A2A2A));
    ImGui::PushStyleColor(ImGuiCol_Border, primary ? theme::Acc(0.75f) : C(0x2C2C2C));
    ImGui::PushStyleColor(ImGuiCol_Text, primary ? C(0x0A0A0A) : C(theme::col::Text));
    const bool clicked = ImGui::Button(label, ImVec2(w, Px(24)));
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    return clicked;
}

void InfoRow(const char* text, unsigned color = theme::col::Dim)
{
    if (!text)
        return;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddText(theme::FontSmall, theme::SizeSmall,
                                        ImVec2(p.x, p.y + Px(2)), C(color), text);
    ImGui::Dummy(ImVec2(w, Px(18)));
}

void InfoWrap(const char* text, unsigned color = theme::col::Dim)
{
    if (!text)
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, C(color));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

// Single-line path with a tail ellipsis when it does not fit. The full value is
// available on hover, so long config folders do not push the layout around.
void InfoPath(const char* path, unsigned color = theme::col::Dim)
{
    if (!path)
        return;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;

    std::string shown = path;
    if (TW(theme::FontSmall, theme::SizeSmall, path) > w - Px(2))
    {
        size_t start = 0;
        const size_t len = shown.size();
        while (start + 3 < len &&
               TW(theme::FontSmall, theme::SizeSmall, shown.c_str() + start) > w - Px(14))
            ++start;
        shown = "..." + shown.substr(start);
    }

    ImGui::GetWindowDrawList()->AddText(theme::FontSmall, theme::SizeSmall,
                                        ImVec2(p.x, p.y + Px(2)), C(color), shown.c_str());
    ImGui::Dummy(ImVec2(w, Px(18)));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", path);
}

void SectionRow(const char* text)
{
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(w, Px(22)));
    if (!text)
        return;
    char up[96];
    ToUpper(text, up, sizeof(up));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    TrackedText(dl, theme::FontSmallBold, theme::SizeSmallBold, ImVec2(p.x, p.y + Px(7)),
                C(theme::col::Dim), up, Px(0.6f));
    const float tw = TrackedWidth(theme::FontSmallBold, theme::SizeSmallBold, up, Px(0.6f));
    dl->AddLine(ImVec2(p.x + tw + Px(8), p.y + Px(12)), ImVec2(p.x + w, p.y + Px(12)),
                C(0x242424), Px(1));
}

void Spacer(float h)
{
    ImGui::Dummy(ImVec2(0.0f, Px(h)));
}

void AccentPresetRow()
{
    struct Preset { const char* name; ImVec4 color; };
    static const Preset presets[4] = {
        { "lime",   ImVec4(149 / 255.f, 212 / 255.f, 84 / 255.f, 1.f) },
        { "azure",  ImVec4(86 / 255.f, 168 / 255.f, 1.f, 1.f) },
        { "violet", ImVec4(214 / 255.f, 92 / 255.f, 214 / 255.f, 1.f) },
        { "ember",  ImVec4(1.f, 138 / 255.f, 72 / 255.f, 1.f) },
    };

    const float w = ImGui::GetContentRegionAvail().x;
    const float gap = Px(6);
    const float bw = (w - gap * 3.0f) / 4.0f;
    const float bh = Px(20);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < 4; ++i)
    {
        ImGui::PushID(i);
        if (i > 0)
            ImGui::SameLine(0.0f, gap);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##preset", ImVec2(bw, bh));
        const bool hov = ImGui::IsItemHovered();
        if (hov)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked(0))
            gs.sAccent = presets[i].color;
        const ImVec2 b(p.x + bw, p.y + bh);
        dl->AddRectFilled(p, b, ImGui::ColorConvertFloat4ToU32(presets[i].color), Px(2));
        dl->AddRect(p, b, SameVec4(gs.sAccent, presets[i].color) ? C(0xF0F0F0)
                                                                 : C(hov ? 0x8A8A8A : 0x353535),
                    Px(2), 0, Px(1));
        if (hov)
            ImGui::SetTooltip("%s", presets[i].name);
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------
// group box
// ---------------------------------------------------------------------------
void GroupBegin(const char* title, float bx, float by, float bw, float bh)
{
    ImGui::SetCursorScreenPos(ImVec2(g_O.x + Px(bx), g_O.y + Px(by)));
    ImGui::PushID(title);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, C(theme::col::Group));
    ImGui::PushStyleColor(ImGuiCol_Border, C(theme::col::Border));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Px(3));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, Px(1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(10), Px(0)));
    ImGui::BeginChild("##group", ImVec2(Px(bw), Px(bh)), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 cp = ImGui::GetCursorScreenPos();
    char up[96];
    ToUpper(title, up, sizeof(up));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    TrackedText(dl, theme::FontSmallBold, theme::SizeSmallBold, ImVec2(cp.x, cp.y + Px(3)),
                C(theme::col::Dim), up, Px(0.7f));
    const float tw = TrackedWidth(theme::FontSmallBold, theme::SizeSmallBold, up, Px(0.7f));
    dl->AddRectFilled(ImVec2(cp.x, cp.y + Px(18)), ImVec2(cp.x + tw, cp.y + Px(20)),
                      theme::Acc(1.0f, 210), Px(1));
    ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + Px(27)));

    cfg::SetGroup(title);
}

void GroupEnd()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// chrome
// ---------------------------------------------------------------------------
void DrawTopChrome(ImVec2 O)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilledMultiColor(O, O + ImVec2(Px(kW), Px(kH)),
                                C(0x131313), C(0x101010), C(0x0B0B0B), C(0x0B0B0B));
    dl->AddRectFilled(O, O + ImVec2(Px(kW), Px(kHeaderH)), C(theme::col::Header));
    dl->AddRectFilled(O + ImVec2(0, Px(kHeaderH)),
                      O + ImVec2(Px(kRailW), Px(kH - kFooterH)), C(theme::col::Sidebar));
    dl->AddRectFilled(O + ImVec2(Px(kRailW), Px(kHeaderH)),
                      O + ImVec2(Px(kW), Px(kH - kFooterH)), C(theme::col::Bg1));
    dl->AddRectFilled(O + ImVec2(0, Px(kH - kFooterH)), O + ImVec2(Px(kW), Px(kH)),
                      C(theme::col::Header));

    dl->AddLine(O + ImVec2(0, Px(kHeaderH)), O + ImVec2(Px(kW), Px(kHeaderH)),
                C(0x1C1C1C), Px(1));
    dl->AddLine(O + ImVec2(Px(kRailW), Px(kHeaderH)),
                O + ImVec2(Px(kRailW), Px(kH - kFooterH)), C(0x262626), Px(2));
    dl->AddLine(O + ImVec2(0, Px(kH - kFooterH)), O + ImVec2(Px(kW), Px(kH - kFooterH)),
                C(0x1C1C1C), Px(1));
    dl->AddLine(O + ImVec2(Px(kRailW), Px(kH - kFooterH)),
                O + ImVec2(Px(kRailW), Px(kH - kFooterH)), C(0x1C1C1C), Px(1));

    // outer hairline
    dl->AddRect(O, O + ImVec2(Px(kW), Px(kH)), C(0x262626), 0.0f, 0, Px(1));

    // rainbow strip (same idea as the reference, drawn lighter)
    const float mid = Px(kW * 0.5f);
    dl->AddRectFilledMultiColor(O, O + ImVec2(mid, Px(2)),
                                C(0x608B99), C(0x854E87), C(0x854E87), C(0x608B99));
    dl->AddRectFilledMultiColor(O + ImVec2(mid, 0), O + ImVec2(Px(kW), Px(2)),
                                C(0x854E87), C(0xB1B580), C(0xB1B580), C(0x854E87));
    dl->AddRectFilled(O, O + ImVec2(Px(kW), Px(1)), C(0x050505));
    dl->AddRectFilled(O + ImVec2(0, Px(3)), O + ImVec2(Px(kW), Px(4)), C(0x050505));
}

void DrawHeader(ImVec2 O)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // window drag: accumulate screen-space cursor deltas. using imgui's
    // window-relative mouse delta would feed the window's own movement back
    // into the next frame and make the window run away.
    static bool dragging = false;
    static POINT dragCursor{};

    ImGui::SetCursorScreenPos(O + ImVec2(Px(6), Px(6)));
    ImGui::InvisibleButton("##drag", ImVec2(Px(kW - 90), Px(kHeaderH - 12)));
    if (ImGui::IsItemClicked(0))
    {
        GetCursorPos(&dragCursor);
        dragging = true;
    }
    if (dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        POINT p{};
        GetCursorPos(&p);
        g_drag.x += (float)(p.x - dragCursor.x);
        g_drag.y += (float)(p.y - dragCursor.y);
        dragCursor = p;
    }
    if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        dragging = false;

    // wordmark
    const float logoY = (Px(kHeaderH) - theme::SizeTitle) * 0.5f - Px(1);
    dl->AddText(theme::FontTitle, theme::SizeTitle, O + ImVec2(Px(16), logoY),
                C(theme::col::TextBright), "skeet");
    const float lw = TW(theme::FontTitle, theme::SizeTitle, "skeet");
    dl->AddText(theme::FontTitle, theme::SizeTitle, O + ImVec2(Px(16) + lw, logoY),
                theme::Acc(), ".");
    dl->AddText(theme::FontSmall, theme::SizeSmall,
                O + ImVec2(Px(16) + lw + Px(8), (Px(kHeaderH) - theme::SizeSmall) * 0.5f + Px(3)),
                C(theme::col::Faint), "ui v2");

    // user + close
    const float closeX = kW - kPad - 22.0f;
    char ubuf[64];
    std::snprintf(ubuf, sizeof(ubuf), "%s", g_user);
    const float nw = TW(theme::FontMain, theme::SizeMain, ubuf);
    const float nx = closeX - Px(16) - nw;
    dl->AddCircleFilled(O + ImVec2(nx - Px(11), Px(kHeaderH * 0.5f)), Px(2.5f),
                        theme::Acc(1.0f, 230));
    dl->AddText(theme::FontMain, theme::SizeMain,
                O + ImVec2(nx, (Px(kHeaderH) - theme::SizeMain) * 0.5f),
                C(theme::col::Dim), ubuf);

    ImGui::SetCursorScreenPos(O + ImVec2(Px(closeX), Px((kHeaderH - 22) * 0.5f)));
    ImGui::InvisibleButton("##close", ImVec2(Px(22), Px(22)));
    const bool ch = ImGui::IsItemHovered();
    if (ch)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked(0))
        g_close = true;

    const ImVec2 ca(O.x + Px(closeX), O.y + Px((kHeaderH - 22) * 0.5f));
    const ImVec2 cb(ca.x + Px(22), ca.y + Px(22));
    if (ch)
    {
        dl->AddRectFilled(ca, cb, C(0x2A1616), Px(2));
        dl->AddRect(ca, cb, C(0x583030), Px(2), 0, Px(1));
    }
    icons::Draw(dl, icons::Id::Close, ImVec2((ca.x + cb.x) * 0.5f, (ca.y + cb.y) * 0.5f),
                Px(11), ch ? C(0xE88888) : C(0x8A8A8A), Px(1.5f));
}

void DrawRail(ImVec2 O)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    for (int i = 0; i < kTabCount; ++i)
    {
        const bool bottom = (i == kTabCount - 1);
        const float y = bottom
            ? (kH - kFooterH - 12.0f - 50.0f)
            : (kHeaderH + 12.0f + (float)i * 54.0f);

        ImGui::SetCursorScreenPos(O + ImVec2(0.0f, Px(y)));
        ImGui::PushID(1000 + i);
        ImGui::InvisibleButton("##rail", ImVec2(Px(kRailW), Px(50)));
        const bool hov = ImGui::IsItemHovered();
        if (hov)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked(0))
            g_tab = i;
        ImGui::PopID();

        const bool sel = (g_tab == i);
        const ImVec2 a = O + ImVec2(0.0f, Px(y));
        const ImVec2 b = O + ImVec2(Px(kRailW), Px(y + 50));

        if (sel)
        {
            dl->AddRectFilled(a, b, theme::Acc(1.0f, 12), Px(0));
            dl->AddRectFilled(a, ImVec2(a.x + Px(2), b.y), theme::Acc(1.0f, 235), Px(1));
        }
        else if (hov)
        {
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 7), Px(0));
        }

        const ImU32 iconCol = sel ? C(0xF2F2F2) : (hov ? C(0xC0C0C0) : C(0x787878));
        icons::Draw(dl, kTabs[i].icon, ImVec2(O.x + Px(kRailW * 0.5f), O.y + Px(y + 18.0f)),
                    Px(21), iconCol, Px(1.5f));

        const float lw = TW(theme::FontSmall, theme::SizeSmall, kTabs[i].name);
        dl->AddText(theme::FontSmall, theme::SizeSmall,
                    ImVec2(O.x + Px(kRailW * 0.5f) - lw * 0.5f, O.y + Px(y + 33.0f)),
                    sel ? theme::Acc() : (hov ? C(0xA0A0A0) : C(theme::col::Dim)), kTabs[i].name);
    }
}

void DrawPageHeader(ImVec2 O)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const TabDef& t = kTabs[g_tab];
    const float invS = 1.0f / theme::S;

    const float tx = kRailW + 1.0f + kPad;
    const float ty = kHeaderH + 13.0f;
    dl->AddText(theme::FontTitle, theme::SizeTitle, O + ImVec2(Px(tx), Px(ty)),
                C(theme::col::TextBright), t.name);
    const float titleW = TW(theme::FontTitle, theme::SizeTitle, t.name) * invS;

    const float sx0 = tx + titleW + 30.0f;
    const float itemY = kHeaderH + 10.0f;
    const float itemH = 26.0f;
    float sx = sx0;
    const int active = g_sub[g_tab];

    for (int i = 0; i < t.subCount; ++i)
    {
        const float tw = TW(theme::FontMain, theme::SizeMain, t.subs[i]) * invS;
        const float iw = tw + 16.0f;

        ImGui::SetCursorScreenPos(O + ImVec2(Px(sx), Px(itemY)));
        ImGui::PushID(2000 + i);
        ImGui::InvisibleButton("##sub", ImVec2(Px(iw), Px(itemH)));
        const bool hov = ImGui::IsItemHovered();
        if (hov)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked(0))
            g_sub[g_tab] = i;
        ImGui::PopID();

        const bool sel = (i == active);
        const ImU32 col = sel ? C(theme::col::TextBright) : (hov ? C(0xC8C8C8) : C(theme::col::Dim));
        dl->AddText(theme::FontMain, theme::SizeMain,
                    O + ImVec2(Px(sx + 8), Px(itemY) + (Px(itemH) - theme::SizeMain) * 0.5f),
                    col, t.subs[i]);

        sx += iw + 6.0f;
    }

    if (t.subCount > 1)
    {
        static float ux = 0.0f;
        static float uw = 0.0f;
        static int lastTab = -1;
        float tgtX = sx0 + 6.0f;
        for (int i = 0; i < active; ++i)
            tgtX += TW(theme::FontMain, theme::SizeMain, t.subs[i]) * invS + 16.0f + 6.0f;
        const float tgtW = TW(theme::FontMain, theme::SizeMain, t.subs[active]) * invS;

        if (lastTab != g_tab)
        {
            lastTab = g_tab;
            ux = tgtX;
            uw = tgtW;
        }
        const float k = ImMin(1.0f, ImGui::GetIO().DeltaTime * 16.0f);
        ux += (tgtX - ux) * k;
        uw += (tgtW - uw) * k;

        const float ly = kHeaderH + 37.0f;
        dl->AddRectFilled(O + ImVec2(Px(ux), Px(ly)),
                          O + ImVec2(Px(ux + uw), Px(ly + 2)),
                          theme::Acc(1.0f, 230), Px(1));
    }

    dl->AddLine(O + ImVec2(Px(kRailW + 1), Px(kHeaderH + kPageHeadH - 1)),
                O + ImVec2(Px(kW), Px(kHeaderH + kPageHeadH - 1)), C(0x181818), Px(1));
}

float Keycap(ImDrawList* dl, ImVec2 O, float xBase, float yBase, const char* text)
{
    const float tw = TW(theme::FontSmall, theme::SizeSmall, text);
    const float w = tw + Px(10);
    const float h = Px(14);
    const ImVec2 a = O + ImVec2(Px(xBase), Px(yBase));
    const ImVec2 b = a + ImVec2(w, h);
    dl->AddRectFilled(a, b, C(0x1A1A1A), Px(2));
    dl->AddRect(a, b, C(0x2E2E2E), Px(2), 0, Px(1));
    dl->AddText(theme::FontSmall, theme::SizeSmall, a + ImVec2(Px(5), Px(2)),
                C(0xB4B4B4), text);
    return w / Px(1);
}

void DrawFooter(ImVec2 O)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    const float y = kH - kFooterH;

    dl->AddText(theme::FontSmall, theme::SizeSmall, O + ImVec2(Px(16), Px(y + 8)),
                C(theme::col::Dim), "skeet ui");
    const float lw = TW(theme::FontSmall, theme::SizeSmall, "skeet ui");
    dl->AddCircleFilled(O + ImVec2(Px(16) + lw + Px(8), Px(y + 14)), Px(1.5f), C(0x3A3A3A));
    dl->AddText(theme::FontSmall, theme::SizeSmall, O + ImVec2(Px(16) + lw + Px(16), Px(y + 8)),
                C(theme::col::Faint), "v2.0");

    float x = 112.0f;
    const float keyY = y + 7.0f;
    const float kw = Keycap(dl, O, x, keyY, "INSERT");
    x += kw + Px(5) / Px(1);
    dl->AddText(theme::FontSmall, theme::SizeSmall, O + ImVec2(Px(x), Px(y + 8)),
                C(theme::col::Faint), "hide");
    x += TW(theme::FontSmall, theme::SizeSmall, "hide") / Px(1) + 12.0f;
    const float kw2 = Keycap(dl, O, x, keyY, "CTRL+F");
    x += kw2 + Px(5) / Px(1);
    dl->AddText(theme::FontSmall, theme::SizeSmall, O + ImVec2(Px(x), Px(y + 8)),
                C(theme::col::Faint), "search");

    // search pill
    const float searchW = 130.0f;
    const float searchX = kW - kPad - searchW;
    ImGui::SetCursorScreenPos(O + ImVec2(Px(searchX), Px(y + 4)));
    ImGui::InvisibleButton("##footerSearch", ImVec2(Px(searchW), Px(20)));
    const bool sh = ImGui::IsItemHovered();
    if (sh)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked(0))
    {
        g_paletteOpen = true;
        g_search[0] = '\0';
        g_focusSearch = true;
    }
    const ImVec2 sa = O + ImVec2(Px(searchX), Px(y + 4));
    const ImVec2 sb = sa + ImVec2(Px(searchW), Px(20));
    dl->AddRectFilled(sa, sb, sh ? C(0x1F1F1F) : C(0x161616), Px(2));
    dl->AddRect(sa, sb, sh ? C(0x3A3A3A) : C(0x262626), Px(2), 0, Px(1));
    icons::Draw(dl, icons::Id::Search, ImVec2(sa.x + Px(11), (sa.y + sb.y) * 0.5f), Px(10),
                C(theme::col::Dim), Px(1.4f));
    dl->AddText(theme::FontSmall, theme::SizeSmall,
                ImVec2(sa.x + Px(21), sa.y + Px(4)), C(theme::col::Dim), "search");
    const float ctrlW = TW(theme::FontSmall, theme::SizeSmall, "ctrl+f");
    dl->AddText(theme::FontSmall, theme::SizeSmall,
                ImVec2(sb.x - Px(7) - ctrlW, sa.y + Px(4)), C(theme::col::Faint), "ctrl+f");

    if (gs.sShowFps)
    {
        char fps[32];
        std::snprintf(fps, sizeof(fps), "%.0f fps", io.Framerate);
        const float fw = TW(theme::FontSmall, theme::SizeSmall, fps);
        dl->AddText(theme::FontSmall, theme::SizeSmall,
                    ImVec2(O.x + Px(searchX) - Px(10) - fw, O.y + Px(y + 8)),
                    C(theme::col::Faint), fps);
    }
}

void DrawToasts(ImVec2 O)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float dt = ImGui::GetIO().DeltaTime;

    int idx = 0;
    for (auto it = g_toasts.rbegin(); it != g_toasts.rend(); ++it, ++idx)
    {
        ToastItem& t = *it;
        t.t += dt;

        const float in = ImClamp(t.t * 7.0f, 0.0f, 1.0f);
        const float out = ImClamp((4.4f - t.t) * 2.4f, 0.0f, 1.0f);
        const float alpha = ImMin(in, out);
        if (alpha <= 0.0f)
            continue;

        const float w = 236.0f;
        const float h = 40.0f;
        const float x = kW - kPad - w;
        const float baseY = kH - kFooterH - 14.0f - h - (float)idx * (h + 6.0f);
        const float yy = baseY + (1.0f - in) * 18.0f + (1.0f - out) * 18.0f;

        const ImVec2 a = O + ImVec2(Px(x), Px(yy));
        const ImVec2 b = a + ImVec2(Px(w), Px(h));
        const int av = (int)(alpha * 255.0f);

        dl->AddRectFilled(a, b, IM_COL32(22, 22, 22, (int)(242.0f * alpha)), Px(3));
        dl->AddRect(a, b, IM_COL32(58, 58, 58, (int)(240.0f * alpha)), Px(3), 0, Px(1));
        const ImU32 bar = t.good ? theme::Acc(1.0f, av) : IM_COL32(224, 90, 90, av);
        dl->AddRectFilled(a + ImVec2(Px(1), Px(3)), a + ImVec2(Px(3), Px(h - 3)), bar, Px(1));

        dl->AddText(theme::FontSmallBold, theme::SizeSmallBold,
                    a + ImVec2(Px(12), Px(8)), IM_COL32(236, 236, 236, av), t.title.c_str());
        dl->AddText(theme::FontSmall, theme::SizeSmall,
                    a + ImVec2(Px(12), Px(22)), IM_COL32(150, 150, 150, av), t.body.c_str());
    }

    g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(),
                                  [](const ToastItem& t) { return t.t > 4.6f; }),
                   g_toasts.end());
}

void AddToast(const char* title, const char* body, bool good)
{
    if (g_toasts.size() >= 4)
        g_toasts.erase(g_toasts.begin());
    ToastItem t;
    t.title = title ? title : "";
    t.body = body ? body : "";
    t.good = good;
    g_toasts.push_back(std::move(t));
}

// ---------------------------------------------------------------------------
// page layout helpers
// ---------------------------------------------------------------------------
float ContentX() { return kRailW + 1.0f + kPad; }
float ContentY() { return kHeaderH + kPageHeadH; }
float ContentW() { return kW - ContentX() - kPad; }
float ContentH() { return kH - kFooterH - ContentY() - kPad; }
float ColW() { return (ContentW() - kColGap) * 0.5f; }

// ---------------------------------------------------------------------------
// pages
// ---------------------------------------------------------------------------
void PageAimbot(int sub)
{
    cfg::SetPage("Aimbot", sub, kTabs[0].subs[sub]);
    const float x0 = ContentX();
    const float x1 = x0 + ColW() + kColGap;
    const float y0 = ContentY();
    const float h = ContentH();
    const float w = ColW();

    if (sub == 0)
    {
        GroupBegin("Aimbot", x0, y0, w, h);
        CheckboxRow("Enabled", gs.aimEnable, "master switch for the aimbot");
        KeyRow("Aim key", gs.aimKey, "hold or toggle depending on mode");
        SliderRowF("FOV", gs.aimFov, 0.0f, 180.0f, "%.0f", "field of view cone in degrees");
        SliderRowF("Smooth", gs.aimSmooth, 1.0f, 20.0f, "%.0f");
        DropdownRow("Hitbox", gs.aimHitbox, kHitbox, 5);
        CheckboxRow("Visible only", gs.aimVisible);
        CheckboxRow("Auto wall", gs.aimWall);
        CheckboxRow("Auto fire", gs.aimAutoFire);
        CheckboxRow("Auto scope", gs.aimAutoScope);
        CheckboxRow("Backtrack", gs.aimBacktrack);
        SliderRowF("Backtrack ms", gs.aimBacktrackMs, 0.0f, 400.0f, "%.0f");
        CheckboxRow("Resolver", gs.aimResolver);
        GroupEnd();

        GroupBegin("Targeting", x1, y0, w, h);
        SliderRowF("Min damage", gs.aimMinDmg, 1.0f, 100.0f, "%.0f");
        SliderRowF("Head scale", gs.aimHeadScale, 0.0f, 100.0f, "%.0f%%");
        DropdownRow("Priority", gs.aimPriority, kPriority, 3);
        CheckboxRow("Multi-point", gs.aimMulti);
        SliderRowF("MP scale", gs.aimMpScale, 0.0f, 100.0f, "%.0f%%");
        CheckboxRow("Team check", gs.aimTeam);
        CheckboxRow("Prediction", gs.aimPredict);
        SliderRowF("Reaction ms", gs.aimReact, 0.0f, 500.0f, "%.0f");
        CheckboxRow("Humanizer", gs.aimHumanize);
        SliderRowF("Smooth curve", gs.aimCurve, 0.0f, 100.0f, "%.0f");
        CheckboxRow("Micro move", gs.aimMicro);
        SliderRowF("Jitter", gs.aimJitter, 0.0f, 5.0f, "%.2f");
        CheckboxRow("Randomize", gs.aimRand);
        GroupEnd();
    }
    else if (sub == 1)
    {
        GroupBegin("Anti-Aim", x0, y0, w, h);
        CheckboxRow("Enabled", gs.aaEnable);
        DropdownRow("Pitch", gs.aaPitch, kPitch, 4);
        DropdownRow("Yaw base", gs.aaYaw, kYaw, 5);
        SliderRowF("Jitter", gs.aaJitter, 0.0f, 60.0f, "%.0f");
        KeyRow("Toggle key", gs.aaKey);
        CheckboxRow("Fake lag", gs.aaFakeLag);
        CheckboxRow("Desync", gs.aaDesync);
        SliderRowF("Desync amount", gs.aaDesyncAmt, 0.0f, 60.0f, "%.0f");
        CheckboxRow("Roll", gs.aaRoll);
        SliderRowF("Roll amount", gs.aaRollAmt, 0.0f, 45.0f, "%.0f");
        CheckboxRow("Hide shots", gs.aaHideShots);
        DropdownRow("Legs", gs.aaLegs, kLegs, 3);
        GroupEnd();

        GroupBegin("Manual AA", x1, y0, w, h);
        CheckboxRow("Manual AA", gs.aaManual, "manual direction keys override base yaw");
        KeyRow("Left", gs.aaLeftKey);
        KeyRow("Right", gs.aaRightKey);
        KeyRow("Back", gs.aaBackKey);
        CheckboxRow("Fake duck", gs.aaFakeDuck);
        SliderRowF("Duck amount", gs.aaDuckAmt, 0.0f, 100.0f, "%.0f");
        CheckboxRow("Slide", gs.aaSlide);
        SliderRowF("Body yaw", gs.aaBodyYaw, 0.0f, 180.0f, "%.0f");
        SliderRowF("Yaw offset", gs.aaYawOff, -180.0f, 180.0f, "%.0f");
        Spacer(2.0f);
        InfoRow("manual keys win over the base yaw", theme::col::Faint);
        InfoRow("desync is clamped server side", theme::col::Faint);
        GroupEnd();
    }
    else
    {
        GroupBegin("Triggerbot", x0, y0, w, h);
        CheckboxRow("Enabled", gs.trEnable);
        KeyRow("Key", gs.trKey);
        DropdownRow("Mode", gs.trMode, kTrigMode, 2);
        SliderRowF("Delay ms", gs.trDelay, 0.0f, 500.0f, "%.0f");
        SliderRowF("Hitchance", gs.trHitchance, 0.0f, 100.0f, "%.0f%%");
        SliderRowF("FOV", gs.trFov, 0.0f, 10.0f, "%.1f");
        DropdownRow("Hitgroup", gs.trHitgroup, kHitgroup, 4);
        CheckboxRow("Crosshair only", gs.trCrosshair);
        CheckboxRow("Scoped only", gs.trScoped);
        GroupEnd();

        GroupBegin("Filters", x1, y0, w, h);
        CheckboxRow("Ignore smoke", gs.trSmoke);
        CheckboxRow("Ignore flash", gs.trFlash);
        CheckboxRow("Auto stop", gs.trAutoStop);
        SliderRowF("Stop speed", gs.trStopSpeed, 0.0f, 100.0f, "%.0f");
        Spacer(4.0f);
        SectionRow("notes");
        InfoWrap("Trigger checks the shared aim cache, including backtrack records and resolver "
                 "state. Filters apply before the delay timer starts.");
        Spacer(2.0f);
        InfoRow("values are demo data", theme::col::Faint);
        GroupEnd();
    }
}

void PageVisuals(int sub)
{
    cfg::SetPage("Visuals", sub, kTabs[1].subs[sub]);
    const float x0 = ContentX();
    const float x1 = x0 + ColW() + kColGap;
    const float y0 = ContentY();
    const float h = ContentH();
    const float w = ColW();

    if (sub == 0)
    {
        GroupBegin("ESP", x0, y0, w, h);
        CheckboxRow("Enabled", gs.espEnable);
        CheckboxRow("Box", gs.espBox);
        CheckboxRow("Name", gs.espName);
        CheckboxRow("Health bar", gs.espHealth);
        CheckboxRow("Weapon", gs.espWeapon);
        CheckboxRow("Distance", gs.espDist);
        DropdownRow("Box style", gs.espBoxStyle, kBoxStyle, 3);
        CheckboxRow("Skeleton", gs.espSkeleton);
        CheckboxRow("Armor bar", gs.espArmor);
        CheckboxRow("Ammo", gs.espAmmo);
        CheckboxRow("Flags", gs.espFlags);
        GroupEnd();

        GroupBegin("Limits", x1, y0, w, h);
        CheckboxRow("Team check", gs.espTeam);
        CheckboxRow("Vis check color", gs.espVisColor);
        SliderRowF("Max distance", gs.espMaxDist, 50.0f, 3000.0f, "%.0f");
        CheckboxRow("Offscreen arrows", gs.espArrows);
        SliderRowF("Fade time", gs.espFade, 0.0f, 10.0f, "%.1f");
        Spacer(4.0f);
        SectionRow("layout");
        InfoRow("colours live on the colours page", theme::col::Faint);
        InfoRow("box styles redraw per frame", theme::col::Faint);
        InfoRow("distance is in game units", theme::col::Faint);
        GroupEnd();
    }
    else if (sub == 1)
    {
        GroupBegin("World", x0, y0, w, h);
        CheckboxRow("Dropped weapons", gs.wWeapons);
        CheckboxRow("C4", gs.wC4);
        CheckboxRow("Grenades", gs.wNades);
        CheckboxRow("Bomb timer", gs.wBombTimer);
        CheckboxRow("Defuse timer", gs.wDefuse);
        DropdownRow("Nade style", gs.wNadeStyle, kNadeStyle, 3);
        CheckboxRow("World glow", gs.wGlow);
        GroupEnd();

        GroupBegin("Overlay", x1, y0, w, h);
        CheckboxRow("Bullet tracers", gs.wTracers);
        CheckboxRow("Impacts", gs.wImpacts);
        CheckboxRow("Hitmarker", gs.wHitmarker);
        CheckboxRow("Damage numbers", gs.wDamage);
        SliderRowF("Fade time", gs.wFade, 0.0f, 6.0f, "%.1f");
        CheckboxRow("Remove smoke", gs.wNoSmoke);
        CheckboxRow("Remove flash", gs.wNoFlash);
        CheckboxRow("Remove decals", gs.wNoDecals);
        GroupEnd();
    }
    else
    {
        GroupBegin("Entity colours", x0, y0, w, h);
        ColorRow("Enemy", gs.cEnemy);
        ColorRow("Team", gs.cTeam);
        ColorRow("Box", gs.cBox);
        ColorRow("Name", gs.cName);
        ColorRow("Skeleton", gs.cSkel);
        ColorRow("Health bar", gs.cHealth);
        Spacer(4.0f);
        SectionRow("preview");
        InfoRow("click a swatch to pick", theme::col::Faint);
        InfoRow("right click resets one", theme::col::Faint);
        GroupEnd();

        GroupBegin("Theme accents", x1, y0, w, h);
        AccentPresetRow();
        Spacer(2.0f);
        ColorRow("Tracer", gs.cTracer);
        ColorRow("Bomb", gs.cBomb);
        ColorRow("Grenade", gs.cGrenade);
        Spacer(2.0f);
        if (ButtonRow("Reset theme colours"))
        {
            gs.sAccent = ImVec4(149 / 255.f, 212 / 255.f, 84 / 255.f, 1.f);
            gs.cTracer = ImVec4(0.58f, 0.83f, 0.33f, 1.f);
            gs.cBomb = ImVec4(1.0f, 0.66f, 0.25f, 1.0f);
            gs.cGrenade = ImVec4(0.56f, 0.78f, 1.0f, 1.0f);
            AddToast("theme reset", "default palette restored", true);
        }
        Spacer(2.0f);
        InfoRow("accent is shared by the menu", theme::col::Faint);
        InfoRow("swatches are saved per config", theme::col::Faint);
        GroupEnd();
    }
}

void PageMisc(int sub)
{
    cfg::SetPage("Misc", sub, kTabs[2].subs[sub]);
    const float x0 = ContentX();
    const float x1 = x0 + ColW() + kColGap;
    const float y0 = ContentY();
    const float h = ContentH();
    const float w = ColW();

    if (sub == 0)
    {
        GroupBegin("Main", x0, y0, w, h);
        CheckboxRow("Bunnyhop", gs.mBhop);
        CheckboxRow("Auto strafe", gs.mStrafe);
        CheckboxRow("Auto accept", gs.mAccept);
        CheckboxRow("Watermark", gs.mWatermark);
        CheckboxRow("Spectator list", gs.mSpecList);
        CheckboxRow("Panic mode", gs.mPanic);
        KeyRow("Panic key", gs.mPanicKey);
        CheckboxRow("Auto pistol", gs.mAutoPistol);
        CheckboxRow("Auto reload", gs.mAutoReload);
        GroupEnd();

        GroupBegin("Identity", x1, y0, w, h);
        CheckboxRow("Clantag", gs.mClantag);
        InputRow("Tag", gs.mClan, (int)sizeof(gs.mClan));
        CheckboxRow("Chat spam", gs.mChatSpam);
        InputRow("Message", gs.mSpam, (int)sizeof(gs.mSpam));
        CheckboxRow("Name changer", gs.mNameChange);
        InputRow("Name", gs.mName, (int)sizeof(gs.mName));
        Spacer(2.0f);
        if (ButtonRow("Apply identity", true))
            AddToast("identity applied", gs.mName, true);
        Spacer(2.0f);
        InfoRow("preview values, not applied", theme::col::Faint);
        GroupEnd();
    }
    else if (sub == 1)
    {
        GroupBegin("Movement", x0, y0, w, h);
        CheckboxRow("Auto peek", gs.mvAutoPeek);
        KeyRow("Peek key", gs.mvAutoPeekKey);
        CheckboxRow("Fast ladder", gs.mvLadder);
        CheckboxRow("Edge jump", gs.mvEdge);
        CheckboxRow("Jump bug", gs.mvJumpBug);
        SliderRowF("Jump chance", gs.mvJumpChance, 0.0f, 100.0f, "%.0f%%");
        CheckboxRow("Auto jump", gs.mvAutoJump);
        GroupEnd();

        GroupBegin("Strafe", x1, y0, w, h);
        CheckboxRow("Air strafe", gs.mvAirStrafe);
        SliderRowF("Strafe speed", gs.mvStrafeSpeed, 0.0f, 100.0f, "%.0f");
        CheckboxRow("Slow walk", gs.mvSlowWalk);
        KeyRow("Slow key", gs.mvSlowKey);
        SliderRowF("Walk speed", gs.mvWalkSpeed, 0.0f, 100.0f, "%.0f");
        SliderRowF("Bhop chance", gs.mvBhopChance, 0.0f, 100.0f, "%.0f%%");
        Spacer(2.0f);
        InfoRow("values are demo data", theme::col::Faint);
        GroupEnd();
    }
    else
    {
        GroupBegin("View", x0, y0, w, h);
        CheckboxRow("Thirdperson", gs.vThird);
        KeyRow("Third key", gs.vThirdKey);
        SliderRowF("Camera dist", gs.vCamDist, 30.0f, 250.0f, "%.0f");
        CheckboxRow("Custom FOV", gs.vFov);
        SliderRowF("FOV", gs.vFovVal, 60.0f, 140.0f, "%.0f");
        CheckboxRow("Viewmodel FOV", gs.vVmFov);
        SliderRowF("VM FOV", gs.vVmFovVal, 54.0f, 100.0f, "%.0f");
        GroupEnd();

        GroupBegin("Effects", x1, y0, w, h);
        CheckboxRow("Remove scope", gs.vNoScope);
        CheckboxRow("Remove zoom", gs.vNoZoom);
        CheckboxRow("Aspect ratio", gs.vAspect);
        SliderRowF("Aspect", gs.vAspectVal, 1.0f, 2.0f, "%.2f");
        CheckboxRow("Motion blur off", gs.vNoBlur);
        CheckboxRow("Flash reduction", gs.vFlashReduce);
        CheckboxRow("Remove punch", gs.vNoPunch);
        GroupEnd();
    }
}

void SkinCard()
{
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = Px(100);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, h));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a = p;
    const ImVec2 b = p + ImVec2(w, h);
    dl->AddRectFilled(a, b, C(0x111111), Px(3));
    dl->AddRect(a, b, C(0x2A2A2A), Px(3), 0, Px(1));
    dl->AddRectFilledMultiColor(a, a + ImVec2(w, Px(3)),
                                theme::Acc(0.28f), theme::Acc(1.0f),
                                theme::Acc(1.0f), theme::Acc(0.28f));

    const char* weapon = kWeapons[ImClamp(gs.skWeapon, 0, 7)];
    const char* skin = kSkins[ImClamp(gs.skSkin, 0, 7)];
    dl->AddText(theme::FontBold, theme::SizeBold, a + ImVec2(Px(12), Px(13)),
                C(theme::col::TextBright), weapon);
    const float ww = TW(theme::FontBold, theme::SizeBold, weapon);
    dl->AddText(theme::FontBold, theme::SizeBold, a + ImVec2(Px(12) + ww, Px(13)),
                C(theme::col::Faint), "  |  ");
    const float pw = TW(theme::FontBold, theme::SizeBold, "  |  ");
    dl->AddText(theme::FontBold, theme::SizeBold, a + ImVec2(Px(12) + ww + pw, Px(13)),
                theme::Acc(), skin);

    dl->AddLine(a + ImVec2(Px(12), Px(42)), ImVec2(b.x - Px(12), a.y + Px(42)), C(0x242424), Px(1));

    char wear[32];
    char seed[32];
    std::snprintf(wear, sizeof(wear), "%.1f%%", gs.skWear);
    std::snprintf(seed, sizeof(seed), "%d", gs.skSeed);
    const char* labels[3] = { "WEAR", "SEED", "STAT TRAK" };
    const char* values[3] = { wear, seed, gs.skStatTrak ? gs.skStat : "off" };
    const float colW = w / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float cx = a.x + Px(12) + colW * (float)i;
        dl->AddText(theme::FontSmall, theme::SizeSmall, ImVec2(cx, a.y + Px(52)),
                    C(theme::col::Dim), labels[i]);
        dl->AddText(theme::FontMain, theme::SizeMain, ImVec2(cx, a.y + Px(66)),
                    C(theme::col::TextBright), values[i]);
    }
    dl->AddText(theme::FontSmall, theme::SizeSmall, a + ImVec2(Px(12), h - Px(17)),
                C(theme::col::Faint), "preview");
}

void PageSkins(int sub)
{
    cfg::SetPage("Skins", sub, kTabs[3].subs[sub]);
    const float x0 = ContentX();
    const float x1 = x0 + ColW() + kColGap;
    const float y0 = ContentY();
    const float h = ContentH();
    const float w = ColW();

    if (sub == 0)
    {
        GroupBegin("Weapon", x0, y0, w, h);
        DropdownRow("Weapon", gs.skWeapon, kWeapons, 8);
        DropdownRow("Skin", gs.skSkin, kSkins, 8);
        SliderRowF("Wear", gs.skWear, 0.0f, 100.0f, "%.1f%%");
        SliderRowI("Seed", gs.skSeed, 0, 1000, "%d");
        InputRow("StatTrak", gs.skStat, (int)sizeof(gs.skStat));
        CheckboxRow("StatTrak enabled", gs.skStatTrak);
        CheckboxRow("Auto apply", gs.skAutoApply);
        CheckboxRow("Equip on spawn", gs.skEquip);
        Spacer(2.0f);
        if (ButtonRow("Apply skin", true))
        {
            char body[96];
            std::snprintf(body, sizeof(body), "%s | %s", kWeapons[ImClamp(gs.skWeapon, 0, 7)],
                          kSkins[ImClamp(gs.skSkin, 0, 7)]);
            AddToast("skin applied", body, true);
        }
        GroupEnd();

        GroupBegin("Preview", x1, y0, w, h);
        SkinCard();
        Spacer(2.0f);
        const float half = ((ImGui::GetContentRegionAvail().x - Px(6)) * 0.5f) / theme::S;
        if (ButtonRow("Previous", false, half))
            gs.skSkin = (gs.skSkin + 7) % 8;
        ImGui::SameLine(0.0f, Px(6));
        if (ButtonRow("Next", false, half))
            gs.skSkin = (gs.skSkin + 1) % 8;
        Spacer(2.0f);
        InfoRow("preview updates live", theme::col::Faint);
        InfoRow("apply only fires a toast here", theme::col::Faint);
        InfoRow("hook the game to make it real", theme::col::Faint);
        GroupEnd();
    }
    else
    {
        GroupBegin("Gloves", x0, y0, w, h);
        CheckboxRow("Enabled", gs.glEnable);
        DropdownRow("Type", gs.glType, kGloves, 6);
        DropdownRow("Skin", gs.glSkin, kGloveSkins, 5);
        SliderRowF("Wear", gs.glWear, 0.0f, 100.0f, "%.1f%%");
        CheckboxRow("Replace default", gs.glReplace);
        CheckboxRow("Force team", gs.glTeam);
        Spacer(2.0f);
        if (ButtonRow("Apply gloves", true))
            AddToast("gloves applied", kGloves[ImClamp(gs.glType, 0, 5)], true);
        GroupEnd();

        GroupBegin("Notes", x1, y0, w, h);
        InfoWrap("Glove models are swapped through the econ item system. The preview page keeps "
                 "the selections local, so nothing is sent to the game.");
        Spacer(4.0f);
        SectionRow("demo");
        InfoRow("replace keeps the default", theme::col::Faint);
        InfoRow("force team uses team skins", theme::col::Faint);
        Spacer(2.0f);
        if (ButtonRow("Refresh models"))
            AddToast("models refreshed", "glove cache rebuilt", true);
        GroupEnd();
    }
}

void PagePlayers()
{
    cfg::SetPage("Players", 0, "Overview");
    const float x0 = ContentX();
    const float x1 = x0 + ColW() + kColGap;
    const float y0 = ContentY();
    const float h = ContentH();
    const float w = ColW();

    struct FakePlayer { const char* name; int hp; bool enemy; int kills; int ping; };
    static const FakePlayer players[8] = {
        { "skibidi", 100, false, 21, 12 },
        { "phoon",    100, true,  18, 24 },
        { "N0thing",   87, true,  14, 41 },
        { "s1mple",   100, true,  23, 9  },
        { "ZywOo",     64, false, 17, 18 },
        { "donk",     100, true,  25, 31 },
        { "m0NESY",    42, false, 11, 55 },
        { "ropz",     100, true,  16, 14 },
    };

    GroupBegin("Players", x0, y0, w, h);
    for (int i = 0; i < 8; ++i)
    {
        ImGui::PushID(i);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float rw = ImGui::GetContentRegionAvail().x;
        const float rh = Px(23);
        ImGui::InvisibleButton("##player", ImVec2(rw, rh));
        const bool hov = ImGui::IsItemHovered();
        if (hov)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked(0))
            g_playerSel = i;
        const bool sel = (g_playerSel == i);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 a = p;
        const ImVec2 b = p + ImVec2(rw, rh);
        if (sel)
        {
            dl->AddRectFilled(a, b, theme::Acc(1.0f, 16), Px(2));
            dl->AddRectFilled(ImVec2(a.x, a.y + Px(4)), ImVec2(a.x + Px(2), b.y - Px(4)),
                              theme::Acc(1.0f, 220), Px(1));
        }
        else if (hov)
        {
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 8), Px(2));
        }

        const ImU32 dotCol = players[i].enemy ? C(0xE05A5A) : C(0x5AA8E0);
        dl->AddCircleFilled(ImVec2(a.x + Px(8), a.y + rh * 0.5f), Px(2.5f), dotCol, 12);
        dl->AddText(theme::FontMain, theme::SizeMain,
                    ImVec2(a.x + Px(18), a.y + (rh - theme::SizeMain) * 0.5f),
                    sel ? C(theme::col::TextBright) : C(theme::col::Text), players[i].name);

        char hp[16];
        std::snprintf(hp, sizeof(hp), "%d hp", players[i].hp);
        const float hpw = TW(theme::FontSmall, theme::SizeSmall, hp);
        dl->AddText(theme::FontSmall, theme::SizeSmall,
                    ImVec2(b.x - hpw, a.y + (rh - theme::SizeSmall) * 0.5f),
                    players[i].enemy ? C(0xB87878) : C(0x78A8B8), hp);
        ImGui::PopID();
    }
    Spacer(2.0f);
    InfoRow("click a player to select", theme::col::Faint);
    GroupEnd();

    const FakePlayer& sel = players[ImClamp(g_playerSel, 0, 7)];
    GroupBegin("Actions", x1, y0, w, h);
    char head[96];
    std::snprintf(head, sizeof(head), "%s  %s", sel.name, sel.enemy ? "enemy" : "team");
    InfoRow(head, theme::col::TextBright);
    InputRow("Spoof name", gs.plSpoof, (int)sizeof(gs.plSpoof));
    CheckboxRow("Reveal ranks", gs.plReveal);
    CheckboxRow("Show bots", gs.plShowBots);
    Spacer(2.0f);
    SectionRow("commands");
    if (ButtonRow("Steal name"))
    {
        std::snprintf(gs.plSpoof, sizeof(gs.plSpoof), "%s", sel.name);
        AddToast("name stolen", sel.name, true);
    }
    if (ButtonRow("Vote kick"))
        AddToast("vote started", sel.name, true);
    if (ButtonRow("Copy steam id"))
        AddToast("copied", "STEAM_1:0:00000000", true);
    Spacer(2.0f);
    char stats[96];
    std::snprintf(stats, sizeof(stats), "kills %d   ping %d ms", sel.kills, sel.ping);
    InfoRow(stats, theme::col::Faint);
    InfoRow("preview roster", theme::col::Faint);
    GroupEnd();
}

void PageConfig(int sub)
{
    cfg::SetPage("Config", sub, kTabs[5].subs[sub]);
    const float x0 = ContentX();
    const float x1 = x0 + ColW() + kColGap;
    const float y0 = ContentY();
    const float h = ContentH();
    const float w = ColW();

    if (sub == 0)
    {
        GroupBegin("Configs", x0, y0, w, h);
        InputRow("Name", g_cfgName, (int)sizeof(g_cfgName));
        if (ButtonRow("Save config", true))
        {
            if (cfg::Save(g_cfgName))
            {
                AddToast("config saved", g_cfgName, true);
                g_cfgRefresh = 0.0f;
            }
            else
                AddToast("save failed", "could not write file", false);
        }
        if (ButtonRow("Load config"))
        {
            if (cfg::Load(g_cfgName))
                AddToast("config loaded", g_cfgName, true);
            else
                AddToast("load failed", "file not found", false);
        }
        if (ButtonRow("Delete config"))
        {
            if (cfg::Delete(g_cfgName))
            {
                AddToast("config deleted", g_cfgName, false);
                g_cfgRefresh = 0.0f;
            }
            else
                AddToast("delete failed", "file not found", false);
        }
        Spacer(2.0f);
        SectionRow("saved");

        if (g_cfgList.empty())
        {
            InfoRow("no configs yet", theme::col::Faint);
        }
        else
        {
            int shown = 0;
            for (const std::string& name : g_cfgList)
            {
                if (shown++ >= 7)
                    break;
                ImGui::PushID(name.c_str());
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const float rw = ImGui::GetContentRegionAvail().x;
                const float rh = Px(23);
                ImGui::InvisibleButton("##cfg", ImVec2(rw, rh));
                const bool ch = ImGui::IsItemHovered();
                if (ch)
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsItemClicked(0))
                {
                    if (cfg::Load(name))
                        AddToast("config loaded", name.c_str(), true);
                }

                ImDrawList* dl = ImGui::GetWindowDrawList();
                const ImVec2 a = p;
                const ImVec2 b = p + ImVec2(rw, rh);
                if (ch)
                    dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 8), Px(2));
                dl->AddText(theme::FontMain, theme::SizeMain,
                            ImVec2(a.x + Px(2), a.y + (rh - theme::SizeMain) * 0.5f),
                            ch ? C(theme::col::TextBright) : C(theme::col::Text), name.c_str());

                const char* hint = "load";
                const float hw = TW(theme::FontSmall, theme::SizeSmall, hint);
                dl->AddText(theme::FontSmall, theme::SizeSmall,
                            ImVec2(b.x - Px(20) - hw, a.y + (rh - theme::SizeSmall) * 0.5f),
                            C(theme::col::Faint), hint);

                ImGui::SetCursorScreenPos(ImVec2(p.x + rw - Px(16), p.y + Px(3)));
                ImGui::InvisibleButton("##del", ImVec2(Px(16), Px(16)));
                const bool dh = ImGui::IsItemHovered();
                if (dh)
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsItemClicked(0))
                {
                    cfg::Delete(name);
                    g_cfgRefresh = 0.0f;
                    AddToast("config deleted", name.c_str(), false);
                }
                icons::Draw(dl, icons::Id::Close,
                            ImVec2(p.x + rw - Px(8), p.y + Px(11)), Px(8),
                            dh ? C(0xE08080) : C(theme::col::Faint), Px(1.2f));

                ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rh + ImGui::GetStyle().ItemSpacing.y));
                ImGui::PopID();
            }
            // The loop leaves the cursor on the next row without submitting an
            // item there, which trips ImGui's SetCursorPos boundary assert.
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }
        GroupEnd();

        GroupBegin("Profile", x1, y0, w, h);
        InfoRow("folder", theme::col::Dim);
        InfoPath(cfg::Dir().c_str(), theme::col::Faint);
        Spacer(2.0f);
        char entries[64];
        std::snprintf(entries, sizeof(entries), "%d entries tracked", (int)cfg::Count());
        InfoRow(entries, theme::col::Faint);
        InfoRow("startup loads 'default'", theme::col::Faint);
        Spacer(4.0f);
        SectionRow("tools");
        if (ButtonRow("Refresh list"))
        {
            g_cfgRefresh = 0.0f;
            AddToast("list refreshed", "configs scanned", true);
        }
        if (ButtonRow("Open configs folder"))
            ShellExecuteA(nullptr, "open", cfg::Dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (ButtonRow("Test toast"))
            AddToast("notification", "toasts stack bottom right", true);
        Spacer(2.0f);
        InfoWrap("Config files are plain text: one key=value per line. Copy them between machines "
                 "to share a profile.", theme::col::Faint);
        GroupEnd();
    }
    else
    {
        GroupBegin("Appearance", x0, y0, w, h);
        ColorRow("Accent", gs.sAccent);
        AccentPresetRow();
        Spacer(2.0f);
        CheckboxRow("Animations", gs.sAnim);
        CheckboxRow("Tooltips", gs.sTips);
        CheckboxRow("Show FPS", gs.sShowFps);
        Spacer(2.0f);
        char dpi[64];
        std::snprintf(dpi, sizeof(dpi), "dpi scale %.0f%%", theme::S * 100.0f);
        InfoRow(dpi, theme::col::Faint);
        Spacer(2.0f);
        if (ButtonRow("Reset appearance"))
        {
            gs.sAccent = ImVec4(149 / 255.f, 212 / 255.f, 84 / 255.f, 1.f);
            gs.sAnim = true;
            gs.sTips = true;
            gs.sShowFps = true;
            AddToast("appearance reset", "defaults restored", true);
        }
        GroupEnd();

        GroupBegin("About", x1, y0, w, h);
        InfoRow("skeet ui v2.0", theme::col::TextBright);
        InfoRow("imgui 1.92.5 / d3d11", theme::col::Faint);
        InfoRow("host agnostic menu framework", theme::col::Faint);
        Spacer(4.0f);
        SectionRow("controls");
        InfoRow("INSERT    hide / show menu", theme::col::Dim);
        InfoRow("CTRL+F    search settings", theme::col::Dim);
        InfoRow("drag      move the window", theme::col::Dim);
        InfoRow("x         close the demo", theme::col::Dim);
        Spacer(4.0f);
        if (ButtonRow("Reload default config"))
        {
            if (cfg::Load("default"))
                AddToast("default loaded", "values restored", true);
            else
                AddToast("no default config", "save one first", false);
        }
        Spacer(2.0f);
        InfoRow("inspired by skeet, rebuilt", theme::col::Faint);
        GroupEnd();
    }
}

void DrawPages()
{
    switch (g_tab)
    {
    case 0: PageAimbot(g_sub[0]); break;
    case 1: PageVisuals(g_sub[1]); break;
    case 2: PageMisc(g_sub[2]); break;
    case 3: PageSkins(g_sub[3]); break;
    case 4: PagePlayers(); break;
    case 5: PageConfig(g_sub[5]); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// search palette
// ---------------------------------------------------------------------------
void JumpTo(const cfg::Entry& e)
{
    for (int i = 0; i < kTabCount; ++i)
    {
        if (e.page == kTabs[i].name)
        {
            g_tab = i;
            g_sub[i] = ImClamp(e.subIndex, 0, kTabs[i].subCount - 1);
            break;
        }
    }
    g_highlightKey = e.key;
    g_highlightT = 3.0f;
    g_paletteOpen = false;
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

void DrawPalette()
{
    if (!g_paletteOpen)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        g_paletteOpen = false;
        return;
    }

    bool hoverPalette = false;
    std::string query = Lower(g_search);
    std::vector<const cfg::Entry*> matches;
    for (const auto& [key, entry] : cfg::Entries())
    {
        if (!query.empty())
        {
            const std::string hay = Lower(entry.label + " " + entry.page + " " + entry.sub + " " + entry.group);
            if (hay.find(query) == std::string::npos)
                continue;
        }
        matches.push_back(&entry);
        if (matches.size() >= 80)
            break;
    }

    if (!matches.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
        JumpTo(*matches.front());

    ImGui::SetNextWindowPos(ImVec2(g_O.x + Px((kW - 440.0f) * 0.5f),
                                  g_O.y + Px((kH - 360.0f) * 0.5f)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(Px(440), Px(360)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(12), Px(12)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, Px(1));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, C(0x151515, 253));
    ImGui::PushStyleColor(ImGuiCol_Border, C(0x323232));

    if (ImGui::Begin("##palette", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        hoverPalette = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        TrackedText(dl, theme::FontSmallBold, theme::SizeSmallBold,
                    wp + ImVec2(Px(12), Px(12)), theme::Acc(), "SEARCH SETTINGS", Px(0.8f));
        dl->AddText(theme::FontSmall, theme::SizeSmall,
                    wp + ImVec2(Px(12), Px(26)), C(theme::col::Faint),
                    "enter jumps to the first match  ·  esc closes");

        ImGui::SetCursorPos(ImVec2(Px(12), Px(44)));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, C(0x1B1B1B));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, C(0x222222));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, C(0x252525));
        ImGui::PushStyleColor(ImGuiCol_Border, C(0x2E2E2E));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Px(8), Px(6)));
        ImGui::SetNextItemWidth(Px(416));
        if (g_focusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            g_focusSearch = false;
        }
        ImGui::InputText("##query", g_search, sizeof(g_search));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        ImGui::SetCursorPos(ImVec2(Px(8), Px(84)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(4), Px(4)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::BeginChild("##results", ImVec2(Px(424), Px(256)), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);

        if (matches.empty())
        {
            ImGui::SetCursorPos(ImVec2(Px(6), Px(6)));
            ImGui::PushStyleColor(ImGuiCol_Text, C(theme::col::Dim));
            ImGui::TextUnformatted("no settings match that query");
            ImGui::PopStyleColor();
        }
        else
        {
            const ImVec2 cp = ImGui::GetCursorScreenPos();
            (void)cp;
            for (const cfg::Entry* e : matches)
            {
                ImGui::PushID(e->key.c_str());
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const float rw = ImGui::GetContentRegionAvail().x;
                const float rh = Px(24);
                ImGui::InvisibleButton("##hit", ImVec2(rw, rh));
                const bool hov = ImGui::IsItemHovered();
                if (hov)
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsItemClicked(0))
                {
                    JumpTo(*e);
                    ImGui::PopID();
                    break;
                }

                ImDrawList* rdl = ImGui::GetWindowDrawList();
                const ImVec2 a = p;
                const ImVec2 b = p + ImVec2(rw, rh);
                if (hov)
                    rdl->AddRectFilled(a, b, theme::Acc(0.5f, 26), Px(2));
                rdl->AddText(theme::FontMain, theme::SizeMain,
                             ImVec2(a.x + Px(6), a.y + (rh - theme::SizeMain) * 0.5f),
                             hov ? C(theme::col::TextBright) : C(theme::col::Text), e->label.c_str());

                std::string path;
                if (e->sub != e->page)
                    path = e->page + " / " + e->sub;
                else
                    path = e->page;
                if (e->group != e->page && e->group != e->sub)
                    path += " / " + e->group;
                if (path.size() > 34)
                    path = path.substr(0, 31) + "...";
                const float pw = TW(theme::FontSmall, theme::SizeSmall, path.c_str());
                rdl->AddText(theme::FontSmall, theme::SizeSmall,
                             ImVec2(b.x - pw - Px(6), a.y + (rh - theme::SizeSmall) * 0.5f),
                             C(theme::col::Faint), path.c_str());
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    if (!hoverPalette && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        g_paletteOpen = false;
}

// Every widget registers itself with the config registry the first time its
// page is drawn. Search must cover settings on pages the user has not opened
// yet, so run one hidden pass over every page/subtab after the first frame.
void WarmupBindings()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    ImGui::SetNextWindowPos(ImVec2(-Px(4000), -Px(4000)));
    ImGui::SetNextWindowSize(ImVec2(Px(kW), Px(kH)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool open = ImGui::Begin(
        "##skeet-bind-warmup", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus);
    if (open)
    {
        const ImVec2 savedOrigin = g_O;
        g_O = ImGui::GetWindowPos();
        for (int i = 0; i < kTabCount; ++i)
        {
            for (int s = 0; s < kTabs[i].subCount; ++s)
            {
                switch (i)
                {
                case 0: PageAimbot(s); break;
                case 1: PageVisuals(s); break;
                case 2: PageMisc(s); break;
                case 3: PageSkins(s); break;
                case 4: PagePlayers(); break;
                case 5: PageConfig(s); break;
                default: break;
                }
            }
        }
        g_O = savedOrigin;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// toast (public wrapper below)
// ---------------------------------------------------------------------------
void RenderImpl()
{
    ImGuiIO& io = ImGui::GetIO();
    g_highlightT = ImMax(0.0f, g_highlightT - io.DeltaTime);
    g_cfgRefresh -= io.DeltaTime;
    if (g_cfgRefresh <= 0.0f)
    {
        g_cfgList = cfg::List();
        g_cfgRefresh = 1.0f;
    }

    if (!SameVec4(gs.sAccent, theme::Accent))
        theme::SetAccent(gs.sAccent);

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false) && !g_paletteOpen)
    {
        g_paletteOpen = true;
        g_search[0] = '\0';
        g_focusSearch = true;
    }

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, C(theme::col::Bg0));

    ImGui::Begin("##skeet-root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoFocusOnAppearing);

    g_O = ImGui::GetWindowPos();

    const bool paletteOpen = g_paletteOpen;
    if (paletteOpen)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
        ImGui::BeginDisabled(true);
    }

    DrawTopChrome(g_O);
    DrawHeader(g_O);
    DrawRail(g_O);
    DrawPageHeader(g_O);
    DrawPages();
    DrawFooter(g_O);
    DrawToasts(g_O);

    if (paletteOpen)
    {
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
    }

    if (g_paletteOpen)
        ImGui::GetWindowDrawList()->AddRectFilled(
            g_O, g_O + ImVec2(Px(kW), Px(kH)), IM_COL32(0, 0, 0, 104));

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    WarmupBindings();
    DrawPalette();
}

} // namespace

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------
void Initialize(float scale)
{
    g_visible = true;
    g_close = false;
    g_drag = ImVec2(0.0f, 0.0f);
    g_tab = 0;
    for (int& s : g_sub)
        s = 0;
    g_highlightT = 0.0f;
    g_highlightKey.clear();
    g_toasts.clear();
    g_paletteOpen = false;
    g_search[0] = '\0';
    g_captureKey = nullptr;
    g_playerSel = 0;
    g_cfgRefresh = 0.0f;

    DWORD n = (DWORD)sizeof(g_user);
    if (!GetUserNameA(g_user, &n))
        std::snprintf(g_user, sizeof(g_user), "user");

    theme::Init(scale);
    theme::SetAccent(gs.sAccent);
    cfg::Init();
}

void Shutdown()
{
}

void Render()
{
    if (!g_visible)
        return;
    RenderImpl();
}

void SetVisible(bool visible)
{
    g_visible = visible;
}

bool IsVisible()
{
    return g_visible;
}

bool ConsumeCloseRequest()
{
    const bool close = g_close;
    g_close = false;
    return close;
}

bool TakeDrag(ImVec2& delta)
{
    if (g_drag.x == 0.0f && g_drag.y == 0.0f)
        return false;
    delta = g_drag;
    g_drag = ImVec2(0.0f, 0.0f);
    return true;
}

void Toast(const char* title, const char* detail, bool good)
{
    AddToast(title, detail, good);
}

} // namespace ui
