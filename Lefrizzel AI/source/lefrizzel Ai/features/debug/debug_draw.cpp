#include "debug_draw.h"

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../../../external/imgui/imgui.h"
#include "../../config/config.h"
#include "../../hooks/hooks.h"
#include "../../interfaces/interfaces.h"
#include "../../utils/console/console.h"
#include "../../utils/memory/memsafe/memsafe.h"
#include "../aim/aim_common.h"
#include "../bones/bones.h"
#include "../engine2/engine2.h"
#include "../trace/trace.h"
#include "../visuals/visuals.h"
#include "../w2s/w2s.h"

namespace DebugDraw {

namespace {

std::atomic<float> g_msPresent{ 0.f };
std::atomic<float> g_msEspCache{ 0.f };
std::atomic<float> g_msHud{ 0.f };
std::atomic<float> g_msMenu{ 0.f };
std::atomic<float> g_msEsp{ 0.f };

constexpr int kRosterMax = 16;
constexpr float kUnitsPerMeter = 39.37f;

// Palette (fixed - debug output must not follow the user's ESP colors).
constexpr ImU32 kInk = IM_COL32(226, 228, 234, 242);
constexpr ImU32 kMuted = IM_COL32(150, 154, 163, 225);
constexpr ImU32 kOk = IM_COL32(126, 220, 146, 240);
constexpr ImU32 kBad = IM_COL32(238, 96, 96, 242);
constexpr ImU32 kWarn = IM_COL32(240, 194, 96, 240);
constexpr ImU32 kPlate = IM_COL32(12, 13, 16, 214);
constexpr ImU32 kBorder = IM_COL32(255, 255, 255, 22);
constexpr ImU32 kSep = IM_COL32(255, 255, 255, 26);

const char* const kHitboxName[] = {
	"head", "neck", "chest", "stomach", "pelvis", "arms", "legs", "feet"
};

struct Canvas {
	ImDrawList* dl = nullptr;
	ImFont* font = nullptr;
	float size = 13.f;
	float lineH = 15.f;
	bool ok = false;

	explicit Canvas(ImDrawList* list) {
		dl = list;
		font = ImGui::GetFont();
		size = ImGui::GetFontSize();
		if (!dl || !font || size < 6.f)
			return;
		lineH = (size + 3.f) > 0.f ? (size + 3.f) : 15.f;
		ok = true;
	}

	float Width(const char* text) const {
		if (!ok || !text)
			return 0.f;
		return font->CalcTextSizeA(size, FLT_MAX, 0.f, text).x;
	}

	void Text(float x, float y, ImU32 col, const char* text) const {
		if (!ok || !text || !text[0])
			return;
		dl->AddText(font, size, ImVec2(x, y), col, text);
	}
};

// Title + six data rows. Kept in one place so the roster panel offset and the
// plate itself cannot drift apart.
constexpr int kStatusLines = 7;
inline float StatusPanelH(const Canvas& c) {
	return 13.f + c.lineH * static_cast<float>(kStatusLines);
}

// Rounded plate with the same accent underline the watermark uses.
void DrawPanel(const Canvas& c, ImVec2 a, ImVec2 b, ImU32 accent) {
	if (!c.ok || b.x <= a.x + 2.f || b.y <= a.y + 2.f)
		return;
	c.dl->AddRectFilled(ImVec2(a.x, a.y + 1.f), ImVec2(b.x, b.y + 2.f), IM_COL32(0, 0, 0, 56), 3.f);
	c.dl->AddRectFilled(a, b, kPlate, 3.f);
	c.dl->AddRect(a, b, kBorder, 3.f, 0, 1.f);
	c.dl->AddRectFilled(ImVec2(a.x + 1.f, b.y - 1.5f), ImVec2(b.x - 1.f, b.y - 0.5f), accent, 1.f);
}

// -- cached slow reads -----------------------------------------------------
struct SlowInfo {
	const char* map = "?";
	int build = 0;
	std::uint64_t stamp = 0;
};
const SlowInfo& Slow() {
	static SlowInfo s{};
	const std::uint64_t now = GetTickCount64();
	if (s.stamp != 0 && now - s.stamp < 1000ull)
		return s;
	s.stamp = now;
	const char* m = Engine2::LevelNameShort();
	s.map = (m && m[0]) ? m : "?";
	s.build = Engine2::BuildNumber();
	return s;
}

int EntityCount() {
	if (!I::GameEntity || !Mem::Valid(I::GameEntity->Instance, 0x2100))
		return 0;
	__try {
		return I::GameEntity->Instance->GetHighestEntityIndex();
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

void DrawStatusBlock(const Canvas& c, ImU32 accent) {
	const float x = 12.f;
	float y = 12.f;
	const float w = 372.f;

	ImGuiIO& io = ImGui::GetIO();
	const float fps = io.Framerate > 0.f ? io.Framerate : 0.f;

	char l0[96]{};
	std::snprintf(l0, sizeof(l0), "debug    %.0f fps   %.1f ms", fps,
		g_msPresent.load(std::memory_order_relaxed));

	char l1[128]{};
	std::snprintf(l1, sizeof(l1), "present %.2f  cache %.2f  hud %.2f  menu %.2f  esp %.2f",
		g_msPresent.load(std::memory_order_relaxed),
		g_msEspCache.load(std::memory_order_relaxed),
		g_msHud.load(std::memory_order_relaxed),
		g_msMenu.load(std::memory_order_relaxed),
		g_msEsp.load(std::memory_order_relaxed));

	const int players = EspPlayersCount();
	const int targets = AimCommon::AimTargetCount();
	char l2[96]{};
	std::snprintf(l2, sizeof(l2), "entities %d   players %d   aimTargets %d",
		EntityCount(), players, targets);

	const int hOk = H::g_hooksInstalled.load(std::memory_order_relaxed);
	const int hAll = H::g_hooksTotal.load(std::memory_order_relaxed);
	const SlowInfo& slow = Slow();
	char l3[128]{};
	std::snprintf(l3, sizeof(l3), "hooks %d/%d   trace %s   build %d",
		hOk, hAll, Trace::Ready() ? "ready" : "off", slow.build);

	char l4[128]{};
	std::snprintf(l4, sizeof(l4), "map %s   %.0fx%.0f   gameFov %.1f",
		slow.map, io.DisplaySize.x, io.DisplaySize.y, H::g_flActiveFov);

	const AimCommon::AimDebugState dbg = AimCommon::AimDebug();
	char l5[160]{};
	if (dbg.valid) {
		const char* hb = (dbg.hitbox >= 0 && dbg.hitbox < Config::HB_COUNT)
			? kHitboxName[dbg.hitbox] : "?";
		std::snprintf(l5, sizeof(l5), "lock %08X   %s   fov %.2f   cand %d   vis %s",
			static_cast<unsigned>(dbg.lockHandle), hb, dbg.fov, dbg.candidates,
			dbg.visible ? "y" : "n");
	} else {
		std::snprintf(l5, sizeof(l5), "no lock   targets %d   cand %d   fovRej %d   visRej %d",
			dbg.targets, dbg.candidates, dbg.fovReject, dbg.visReject);
	}

	const char* rows[6] = { l0, l1, l2, l3, l4, l5 };
	float maxW = 0.f;
	for (const char* r : rows) {
		const float ww = c.Width(r);
		if (ww > maxW)
			maxW = ww;
	}
	const float panelW = (std::max)(w, maxW + 20.f);
	const float panelH = StatusPanelH(c);
	DrawPanel(c, ImVec2(x, y), ImVec2(x + panelW, y + panelH), accent);

	float ty = y + 7.f;
	c.Text(x + 8.f, ty, accent, "SKID.CC // DEBUG");
	ty += c.lineH;
	c.Text(x + 8.f, ty, kMuted, l0);
	ty += c.lineH;
	c.Text(x + 8.f, ty, kMuted, l1);
	ty += c.lineH;
	c.Text(x + 8.f, ty, kInk, l2);
	ty += c.lineH;
	c.Text(x + 8.f, ty, (hAll > 0 && hOk >= hAll) ? kOk : kWarn, l3);
	ty += c.lineH;
	c.Text(x + 8.f, ty, kMuted, l4);
	ty += c.lineH;
	c.Text(x + 8.f, ty, dbg.valid ? (dbg.visible ? kOk : kWarn) : kMuted, l5);
}

void DrawRoster(const Canvas& c, ImU32 accent, float topY) {
	static PlayerCache buf[kRosterMax]{};
	const int n = EspPlayersSnapshot(buf, kRosterMax);
	if (n <= 0)
		return;

	const float x = 12.f;
	const float w = 372.f;
	const float headH = c.lineH + 6.f;
	const float rowH = c.lineH;
	// title + column header + n rows, plus padding.
	float panelH = 8.f + headH + rowH * static_cast<float>(n + 1);

	const ImVec2 a(x, topY);
	const ImVec2 b(x + w, topY + panelH);
	DrawPanel(c, a, b, accent);

	float ty = topY + 5.f;
	char title[48]{};
	std::snprintf(title, sizeof(title), "PLAYERS %d", n);
	c.Text(x + 8.f, ty, accent, title);
	ty += headH;

	const bool haveLocal = cached_local.position.Length() > 1.f;
	const float colName = x + 8.f;
	const float colHp = x + 214.f;
	const float colDist = x + 262.f;
	const float colVis = x + 330.f;

	c.Text(colName, ty, kMuted, "name");
	c.Text(colHp, ty, kMuted, "hp");
	c.Text(colDist, ty, kMuted, "dist");
	c.Text(colVis, ty, kMuted, "vis");
	c.dl->AddLine(ImVec2(x + 4.f, ty + c.lineH - 3.f), ImVec2(x + w - 4.f, ty + c.lineH - 3.f), kSep, 1.f);
	ty += rowH;

	for (int i = 0; i < n; ++i) {
		const PlayerCache& p = buf[i];
		const ImU32 nameCol = p.visible ? kInk : kMuted;

		char name[40]{};
		std::snprintf(name, sizeof(name), "%.28s", (p.name[0] ? p.name : "?"));
		c.Text(colName, ty, nameCol, name);

		char hp[16]{};
		std::snprintf(hp, sizeof(hp), "%d", p.health);
		c.Text(colHp, ty, p.health <= 25 ? kBad : (p.health <= 60 ? kWarn : kOk), hp);

		char dist[16]{};
		if (haveLocal) {
			const float units = (p.position - cached_local.position).Length();
			if (std::isfinite(units) && units < 100000.f)
				std::snprintf(dist, sizeof(dist), "%.0fm", units / kUnitsPerMeter);
			else
				std::snprintf(dist, sizeof(dist), "--");
		} else {
			std::snprintf(dist, sizeof(dist), "--");
		}
		c.Text(colDist, ty, kMuted, dist);

		c.Text(colVis, ty, p.visible ? kOk : kBad, p.visible ? "y" : "n");
		ty += rowH;
	}
}

void DrawAimTarget(const Canvas& c, ImU32 accent) {
	const AimCommon::AimDebugState dbg = AimCommon::AimDebug();
	if (!dbg.valid || dbg.stampMs == 0)
		return;
	// Stale scans (key released) must not leave a marker sitting on screen.
	if (GetTickCount64() > dbg.stampMs + 400ull)
		return;
	if (!Bones::IsValidPos(dbg.point))
		return;

	Vector_t screen{};
	if (!W2S::WorldToScreen(dbg.point, screen))
		return;
	if (!std::isfinite(screen.x) || !std::isfinite(screen.y))
		return;

	const ImVec2 p(screen.x, screen.y);
	const float r = 10.f;
	const ImU32 col = dbg.visible ? accent : kWarn;
	c.dl->AddQuad(
		ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y),
		ImVec2(p.x + r * 0.55f, p.y + r * 0.55f), ImVec2(p.x - r * 0.55f, p.y + r * 0.55f),
		col, 1.4f);
	c.dl->AddCircle(p, r * 0.55f, col, 16, 1.2f);

	char label[96]{};
	const char* hb = (dbg.hitbox >= 0 && dbg.hitbox < Config::HB_COUNT)
		? kHitboxName[dbg.hitbox] : "?";
	std::snprintf(label, sizeof(label), "%s  %.1f", hb, dbg.fov);
	const float tw = c.Width(label);
	c.dl->AddRectFilled(ImVec2(p.x - tw * 0.5f - 4.f, p.y - r - c.lineH - 3.f),
		ImVec2(p.x + tw * 0.5f + 4.f, p.y - r - 1.f), kPlate, 2.f);
	c.Text(p.x - tw * 0.5f, p.y - r - c.lineH - 1.f, col, label);
}

void DrawAimRay(const Canvas& c) {
	const AimCommon::AimDebugState dbg = AimCommon::AimDebug();
	if (!dbg.valid || dbg.stampMs == 0)
		return;
	if (GetTickCount64() > dbg.stampMs + 400ull)
		return;
	C_CSPlayerPawn* lp = H::SafeLocalPlayer();
	if (!lp || !Mem::ValidEntity(lp) || !Bones::IsValidPos(dbg.point))
		return;
	const Vector_t eye = Bones::GetShootPos(lp);
	if (!Bones::IsValidPos(eye))
		return;

	Vector_t a{}, b{};
	if (!W2S::WorldToScreen(eye, a) || !W2S::WorldToScreen(dbg.point, b))
		return;
	if (!std::isfinite(a.x) || !std::isfinite(b.x))
		return;
	c.dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y),
		dbg.visible ? IM_COL32(110, 210, 130, 200) : IM_COL32(235, 95, 95, 210), 1.2f);
}

void DrawHitboxDots(const Canvas& c, ImU32 accent) {
	const int n = AimCommon::AimTargetCount();
	if (n <= 0)
		return;
	const AimCommon::AimTarget* list = AimCommon::AimTargets();
	if (!list)
		return;
	const int cap = (n < 16) ? n : 16;
	for (int i = 0; i < cap; ++i) {
		C_CSPlayerPawn* pawn = list[i].pawn;
		if (!pawn || !Mem::ValidEntity(pawn))
			continue;
		for (int hb = 0; hb < Config::HB_COUNT; ++hb) {
			if (!Config::aim_hitboxes[hb])
				continue;
			Vector_t pt{};
			if (!Bones::GetHitboxPoint(pawn, hb, pt) || !Bones::IsValidPos(pt))
				continue;
			Vector_t s{};
			if (!W2S::WorldToScreen(pt, s))
				continue;
			if (!std::isfinite(s.x) || !std::isfinite(s.y))
				continue;
			const float r = (hb == Config::HB_HEAD) ? 3.2f : 2.2f;
			c.dl->AddCircleFilled(ImVec2(s.x, s.y), r, accent, 12);
			c.dl->AddCircle(ImVec2(s.x, s.y), r + 1.4f, IM_COL32(0, 0, 0, 140), 12, 1.f);
		}
	}
}

} // namespace

void SetFrameTimes(float presentMs, float espCacheMs, float hudMs, float menuMs, float espMs) {
	g_msPresent.store(presentMs, std::memory_order_relaxed);
	g_msEspCache.store(espCacheMs, std::memory_order_relaxed);
	g_msHud.store(hudMs, std::memory_order_relaxed);
	g_msMenu.store(menuMs, std::memory_order_relaxed);
	g_msEsp.store(espMs, std::memory_order_relaxed);
}

FrameTimes Times() {
	FrameTimes t{};
	t.present = g_msPresent.load(std::memory_order_relaxed);
	t.espCache = g_msEspCache.load(std::memory_order_relaxed);
	t.hud = g_msHud.load(std::memory_order_relaxed);
	t.menu = g_msMenu.load(std::memory_order_relaxed);
	t.esp = g_msEsp.load(std::memory_order_relaxed);
	return t;
}

void Render(ImDrawList* dl) {
	if (!dl)
		return;
	if (!Config::debug_overlay && !Config::debug_players
		&& !Config::debug_aim_target && !Config::debug_aim_ray && !Config::debug_hitboxes)
		return;
	// Hud::render calls this outside the in-session gate so the block can follow
	// the menu; keep it off in the lobby unless explicitly requested.
	const bool inSession = H::SessionEntityReady() && !H::SessionMapLeaving()
		&& !H::SessionPostMatch();
	if (!inSession && !Config::debug_always)
		return;

	Canvas c(dl);
	if (!c.ok)
		return;

	const ImU32 accent = ImGui::ColorConvertFloat4ToU32(Config::menu_accent);

	if (Config::debug_overlay)
		DrawStatusBlock(c, accent);

	if (Config::debug_players) {
		// Roster sits under the status block when that is on, else at the top.
		const float top = Config::debug_overlay
			? (12.f + StatusPanelH(c) + 8.f)
			: 12.f;
		DrawRoster(c, accent, top);
	}
	if (Config::debug_hitboxes)
		DrawHitboxDots(c, accent);
	if (Config::debug_aim_ray)
		DrawAimRay(c);
	if (Config::debug_aim_target)
		DrawAimTarget(c, accent);
}

void DumpStateToLog() {
	Con::Ok("---- DEBUG DUMP ----");
	Con::Ok("frame: fps %.1f present %.2f cache %.2f hud %.2f menu %.2f esp %.2f",
		ImGui::GetIO().Framerate,
		g_msPresent.load(std::memory_order_relaxed),
		g_msEspCache.load(std::memory_order_relaxed),
		g_msHud.load(std::memory_order_relaxed),
		g_msMenu.load(std::memory_order_relaxed),
		g_msEsp.load(std::memory_order_relaxed));
	Con::Ok("display: %.0fx%.0f  gameFov %.2f  uiScale %d%%",
		ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y,
		H::g_flActiveFov, Config::menu_dpi_scale);
	Con::Ok("env: build %d  map %s  entities %d",
		Engine2::BuildNumber(), Slow().map, EntityCount());
	const int hOk = H::g_hooksInstalled.load(std::memory_order_relaxed);
	const int hAll = H::g_hooksTotal.load(std::memory_order_relaxed);
	Con::Ok("hooks: %d/%d  trace=%d  w2s=%d  schema-ready=%d",
		hOk, hAll, Trace::Ready() ? 1 : 0, W2S::HasCamera() ? 1 : 0, W2S::Init() ? 1 : 0);

	static PlayerCache buf[kRosterMax]{};
	const int n = EspPlayersSnapshot(buf, kRosterMax);
	Con::Ok("players cached: %d  local alive=%d hp=%d team=%d",
		n, cached_local.alive ? 1 : 0, cached_local.health, cached_local.team);
	for (int i = 0; i < n; ++i) {
		const PlayerCache& p = buf[i];
		const float units = cached_local.position.Length() > 1.f
			? (p.position - cached_local.position).Length() : -1.f;
		Con::Ok("  [%d] %-24s hp=%3d team=%d vis=%d dist=%.0fm", i,
			p.name[0] ? p.name : "?", p.health, p.team_num,
			p.visible ? 1 : 0, units * 0.0254f);
	}

	const AimCommon::AimDebugState dbg = AimCommon::AimDebug();
	Con::Ok("aim: valid=%d lock=%08X hb=%d fov=%.2f targets=%d cand=%d fovRej=%d visRej=%d",
		dbg.valid ? 1 : 0, static_cast<unsigned>(dbg.lockHandle), dbg.hitbox, dbg.fov,
		dbg.targets, dbg.candidates, dbg.fovReject, dbg.visReject);
	Con::Ok("aim cfg: enabled=%d key=%d fov=%.2f smooth=%.1f vis=%d team=%d hitboxes=%d%d%d%d%d%d%d%d",
		Config::aimbot ? 1 : 0, Config::aimbot_key, Config::aimbot_fov, Config::aimbot_smooth,
		Config::aim_vis_check ? 1 : 0, Config::team_check ? 1 : 0,
		Config::aim_hitboxes[0] ? 1 : 0, Config::aim_hitboxes[1] ? 1 : 0,
		Config::aim_hitboxes[2] ? 1 : 0, Config::aim_hitboxes[3] ? 1 : 0,
		Config::aim_hitboxes[4] ? 1 : 0, Config::aim_hitboxes[5] ? 1 : 0,
		Config::aim_hitboxes[6] ? 1 : 0, Config::aim_hitboxes[7] ? 1 : 0);
	Con::Ok("esp cfg: esp=%d vis=%d fovCircle=%d players=%d overlay=%d",
		Config::esp ? 1 : 0, Config::esp_vis_check ? 1 : 0, Config::fov_circle ? 1 : 0,
		Config::debug_players ? 1 : 0, Config::debug_overlay ? 1 : 0);
	Con::Ok("---- END DEBUG DUMP ----");
}

} // namespace DebugDraw
