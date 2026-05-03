#pragma once

#include "mod_base.hpp"

namespace mods {

/* Minimal compatibility subset: strip ^...^ markup to plain text.
 * This is not a full status2d renderer implementation. */
inline const char* status2d_plain(const char *src) noexcept
{
	static char out[256];
	if (!src) {
		out[0] = '\0';
		return out;
	}
	unsigned int j = 0;
	for (unsigned int i = 0; src[i] != '\0' && j + 1 < sizeof(out); ++i) {
		if (src[i] == '^') {
			++i;
			while (src[i] && src[i] != '^') ++i;
			continue;
		}
		out[j++] = src[i];
	}
	out[j] = '\0';
	return out;
}

struct Status2DMod : ModBase {};

} // namespace mods
