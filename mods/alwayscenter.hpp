#pragma once

#include "mod_base.hpp"

namespace mods {

struct AlwaysCenterMod : ModBase {
	template <typename ClientT>
	inline static void after_manage(ClientT& c) noexcept
	{
		if (!c.mon || !c.isfloating || c.isfullscreen)
			return;
		const int nx = c.mon->wx + (c.mon->ww - WIDTH(&c)) / 2;
		const int ny = c.mon->wy + (c.mon->wh - HEIGHT(&c)) / 2;
		resize(&c, nx, ny, c.w, c.h, 0);
	}
};

} // namespace mods
