#pragma once

#include "mod_base.hpp"

namespace mods {

inline constexpr unsigned int scratchtag() noexcept { return 1u << 20; }

inline Client* find_scratchpad() noexcept
{
	for (Monitor *m = mons; m; m = m->next)
		for (Client *c = m->clients; c; c = c->next)
			if (c->tags & scratchtag())
				return c;
	return nullptr;
}

inline void scratchpad_toggle(const Arg *) noexcept
{
	Client *c = find_scratchpad();
	if (!c) {
		static const char *scratch_spawn_cmd[] = { "st", "-t", "scratchpad", nullptr };
		Arg a = { .v = scratch_spawn_cmd };
		spawn(&a);
		return;
	}
	if (ISVISIBLE(c)) {
		c->tags = 0;
		focus(nullptr);
		arrange(c->mon);
		return;
	}
	c->tags = selmon->tagset[selmon->seltags] | scratchtag();
	if (c->mon != selmon)
		sendmon(c, selmon);
	focus(c);
	arrange(selmon);
}

struct ScratchpadMod : ModBase {
	template <typename ClientT>
	inline static void after_manage(ClientT& c) noexcept
	{
		if (strstr(c.name, "scratchpad") != nullptr) {
			c.tags = scratchtag();
			c.isfloating = 1;
			if (c.mon)
				resize(&c, c.mon->wx + c.mon->ww / 8, c.mon->wy + c.mon->wh / 8,
					(c.mon->ww * 3) / 4, (c.mon->wh * 3) / 4, 0);
		}
	}
};

} // namespace mods

template <>
struct CommandResolver<"scratchpad:toggle"> { static consteval void (*resolve())(const Arg *) { return &mods::scratchpad_toggle; } };
