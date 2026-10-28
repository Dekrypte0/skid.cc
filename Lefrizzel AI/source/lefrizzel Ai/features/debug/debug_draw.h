#pragma once

struct ImDrawList;

// In-game diagnostics: the stuff the Debug tab shows in the menu, drawn on the
// overlay so it is readable while actually playing (menu closed). Everything is
// gated by Config::debug_* so it costs nothing when off.
namespace DebugDraw {

// Present timing, published by main.cpp every frame. Cheap atomics.
void SetFrameTimes(float presentMs, float espCacheMs, float hudMs, float menuMs, float espMs);

struct FrameTimes {
	float present = 0.f;
	float espCache = 0.f;
	float hud = 0.f;
	float menu = 0.f;
	float esp = 0.f;
};
FrameTimes Times();

// Called from Hud::render() while a session is live. Never throws: the caller
// wraps it in SEH like the other HUD paths.
void Render(ImDrawList* dl);

// Writes the full diagnostic block (same numbers the tab shows) to
// %TEMP%\skidcc_log.txt. Used by the "Dump state" button.
void DumpStateToLog();

} // namespace DebugDraw
