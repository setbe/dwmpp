#pragma once

#include "mod_base.hpp"
#include "../core/mod_state.hpp"

namespace mods {

struct GapsState {
	int px = 8;
	int enabled = 1;
};

using GapsStorage = core::ModState<struct GapsStateTag, GapsState>;
inline GapsState& gaps_state() noexcept { return GapsStorage::value; }

inline int gaps_px() noexcept
{
	GapsState& st = gaps_state();
	return st.enabled ? (st.px < 0 ? 0 : st.px) : 0;
}

inline void gaps_increase(const Arg *arg) noexcept
{
	int delta = (arg && arg->i != 0) ? arg->i : 1;
	GapsState& st = gaps_state();
	st.px += delta;
	if (st.px < 0)
		st.px = 0;
	arrange(selmon);
}

inline void gaps_decrease(const Arg *arg) noexcept
{
	int delta = (arg && arg->i != 0) ? arg->i : 1;
	Arg a = {0};
	a.i = -delta;
	gaps_increase(&a);
}

inline void gaps_toggle(const Arg *) noexcept
{
	GapsState& st = gaps_state();
	st.enabled = !st.enabled;
	arrange(selmon);
}

inline void gaps_reset(const Arg *) noexcept
{
	GapsState& st = gaps_state();
	st.px = 8;
	st.enabled = 1;
	arrange(selmon);
}

struct GapsMod : ModBase {};

} // namespace mods

template <>
struct CommandResolver<"gaps:increase"> {
	static consteval void (*resolve())(const Arg *) { return &mods::gaps_increase; }
};
template <>
struct CommandResolver<"gaps:decrease"> {
	static consteval void (*resolve())(const Arg *) { return &mods::gaps_decrease; }
};
template <>
struct CommandResolver<"gaps:toggle"> {
	static consteval void (*resolve())(const Arg *) { return &mods::gaps_toggle; }
};
template <>
struct CommandResolver<"gaps:reset"> {
	static consteval void (*resolve())(const Arg *) { return &mods::gaps_reset; }
};
