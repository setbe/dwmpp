#pragma once

#include <cstdint>

namespace core {

struct WindowId { std::uint64_t value; };
struct MonitorId { std::int32_t value; };
struct TagMask { std::uint32_t value; };
struct BorderWidth { std::int32_t value; };
struct LayoutId { std::int32_t value; };

struct Position {
	std::int32_t x;
	std::int32_t y;
};

struct Size {
	std::int32_t w;
	std::int32_t h;
};

struct Geometry {
	Position pos;
	Size size;
};

static_assert(sizeof(TagMask) == sizeof(std::uint32_t), "TagMask size must match u32");

} // namespace core
