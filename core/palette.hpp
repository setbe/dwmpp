#pragma once

#include <cstddef>
#include <cstdint>

#include "../drw.h"

namespace core {

struct Color {
	float r;
	float g;
	float b;
	float a;
};

enum class ColorId : std::uint8_t {
	Bg,
	Fg,
	Border,
	Accent,
	Urgent,
	TagActive,
	TagInactive,
	StatusFg,
	StatusBg,
	COUNT
};

inline constexpr std::size_t color_count = static_cast<std::size_t>(ColorId::COUNT);

struct PaletteData {
	Color colors[color_count];
};

struct XColorCached {
	Clr xft{};
	bool valid{false};
};

inline Color color_clamp(Color c) noexcept
{
	if (c.r < 0.f) c.r = 0.f; else if (c.r > 1.f) c.r = 1.f;
	if (c.g < 0.f) c.g = 0.f; else if (c.g > 1.f) c.g = 1.f;
	if (c.b < 0.f) c.b = 0.f; else if (c.b > 1.f) c.b = 1.f;
	if (c.a < 0.f) c.a = 0.f; else if (c.a > 1.f) c.a = 1.f;
	return c;
}

inline Color color_darken(Color c, float k) noexcept
{
	if (k < 0.f) k = 0.f;
	if (k > 1.f) k = 1.f;
	c.r *= (1.f - k);
	c.g *= (1.f - k);
	c.b *= (1.f - k);
	return color_clamp(c);
}

inline Color color_lighten(Color c, float k) noexcept
{
	if (k < 0.f) k = 0.f;
	if (k > 1.f) k = 1.f;
	c.r = c.r + (1.f - c.r) * k;
	c.g = c.g + (1.f - c.g) * k;
	c.b = c.b + (1.f - c.b) * k;
	return color_clamp(c);
}

inline Color color_mix(Color a, Color b, float t) noexcept
{
	if (t < 0.f) t = 0.f;
	if (t > 1.f) t = 1.f;
	Color out{
		a.r + (b.r - a.r) * t,
		a.g + (b.g - a.g) * t,
		a.b + (b.b - a.b) * t,
		a.a + (b.a - a.a) * t,
	};
	return color_clamp(out);
}

inline float color_distance(Color a, Color b) noexcept
{
	const float dr = a.r - b.r;
	const float dg = a.g - b.g;
	const float db = a.b - b.b;
	return dr * dr + dg * dg + db * db;
}

inline bool is_readable(Color fg, Color bg) noexcept
{
	return color_distance(fg, bg) >= 0.09f;
}

class Palette {
public:
	void reset(const PaletteData &d) noexcept
	{
		for (std::size_t i = 0; i < color_count; ++i) {
			colors_[i] = color_clamp(d.colors[i]);
			cache_[i].valid = false;
		}
	}

	Color get(ColorId id) const noexcept { return colors_[static_cast<std::size_t>(id)]; }

	bool try_set(ColorId id, Color c) noexcept
	{
		colors_[static_cast<std::size_t>(id)] = color_clamp(c);
		cache_[static_cast<std::size_t>(id)].valid = false;
		return true;
	}

	const Clr* xft(Drw *drw, ColorId id) const noexcept
	{
		const std::size_t i = static_cast<std::size_t>(id);
		if (!cache_[i].valid) {
			Color c = colors_[i];
			drw_clr_create_rgba(drw, &cache_[i].xft, DrwColorF{c.r, c.g, c.b, c.a});
			cache_[i].valid = true;
		}
		return &cache_[i].xft;
	}

	void invalidate() noexcept
	{
		for (std::size_t i = 0; i < color_count; ++i)
			cache_[i].valid = false;
	}

private:
	Color colors_[color_count]{};
	mutable XColorCached cache_[color_count]{};
};

} // namespace core
