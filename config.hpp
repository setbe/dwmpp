#pragma once

/* Stage-1 compatibility bridge: legacy compile-time config source */
#include "core/palette.hpp"
#include "config.h"

namespace core {
inline constexpr PaletteData default_palette = {{
	{0.10f, 0.10f, 0.10f, 1.00f}, /* Bg */
	{0.90f, 0.90f, 0.90f, 1.00f}, /* Fg */
	{0.30f, 0.22f, 0.31f, 1.00f}, /* Border */
	{0.40f, 0.09f, 0.19f, 1.00f}, /* Accent */
	{0.85f, 0.20f, 0.20f, 1.00f}, /* Urgent */
	{0.94f, 0.65f, 0.87f, 1.00f}, /* TagActive */
	{0.62f, 0.48f, 0.77f, 1.00f}, /* TagInactive */
	{0.94f, 0.65f, 0.87f, 1.00f}, /* StatusFg */
	{0.10f, 0.10f, 0.10f, 1.00f}, /* StatusBg */
}};
} // namespace core

namespace core {
template <>
struct ActiveModsSelector<void> {
	using type = ModList<
#if MOD_ENABLE_ALWAYSCENTER
		mods::AlwaysCenterMod,
#endif
#if MOD_ENABLE_ATTACH_MODES
		mods::AttachModesMod,
#endif
#if MOD_ENABLE_FAKEFULLSCREEN
		mods::FakeFullscreenMod,
#endif
#if MOD_ENABLE_GAPS
		mods::GapsMod,
#endif
#if MOD_ENABLE_MOVESTACK
		mods::MoveStackMod,
#endif
#if MOD_ENABLE_PERTAG
		mods::PertagMod,
#endif
#if MOD_ENABLE_SCRATCHPAD
		mods::ScratchpadMod,
#endif
#if MOD_ENABLE_STATUS2D
		mods::Status2DMod,
#endif
#if MOD_ENABLE_TITLESTATS
		mods::TitleStatsMod,
#endif
		core::NoMod
	>;
};
} // namespace core
