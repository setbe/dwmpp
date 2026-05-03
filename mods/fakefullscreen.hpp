#pragma once

#include "mod_base.hpp"

namespace mods {

inline int& fakefullscreen_enabled() noexcept
{
	static int enabled = 1;
	return enabled;
}

inline int& fakefullscreen_state(Window w) noexcept
{
	static struct { Window w; int enabled; } slots[256]{};
	for (auto &slot : slots) {
		if (slot.w == w) return slot.enabled;
	}
	for (auto &slot : slots) {
		if (slot.w == 0) {
			slot.w = w;
			slot.enabled = 0;
			return slot.enabled;
		}
	}
	return slots[0].enabled;
}

inline int is_fakefullscreen(Client *c) noexcept
{
	if (!fakefullscreen_enabled() || !c) return 0;
	return fakefullscreen_state(c->win);
}

inline void toggle_fakefullscreen(const Arg *) noexcept
{
	if (!selmon || !selmon->sel) return;
	Client *c = selmon->sel;
	int &state = fakefullscreen_state(c->win);
	state = !state;
	if (state) {
		c->isfullscreen = 1;
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
	} else {
		c->isfullscreen = 0;
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
	}
	arrange(selmon);
}

inline void fakefullscreen_toggle_global(const Arg *) noexcept
{
	fakefullscreen_enabled() = !fakefullscreen_enabled();
}

struct FakeFullscreenMod : ModBase {};

} // namespace mods

template <>
struct CommandResolver<"fakefullscreen:toggle"> { static consteval void (*resolve())(const Arg *) { return &mods::toggle_fakefullscreen; } };
template <>
struct CommandResolver<"fakefullscreen:toggle_global"> { static consteval void (*resolve())(const Arg *) { return &mods::fakefullscreen_toggle_global; } };
