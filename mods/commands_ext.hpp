#pragma once

/*
 * Extension point for mods: add CommandResolver specializations here,
 * or in mod headers included by this file.
 */

#ifndef MOD_ENABLE_ALWAYSCENTER
#define MOD_ENABLE_ALWAYSCENTER 1
#endif
#ifndef MOD_ENABLE_ATTACH_MODES
#define MOD_ENABLE_ATTACH_MODES 1
#endif
#ifndef MOD_ENABLE_FAKEFULLSCREEN
#define MOD_ENABLE_FAKEFULLSCREEN 1
#endif
#ifndef MOD_ENABLE_GAPS
#define MOD_ENABLE_GAPS 1
#endif
#ifndef MOD_ENABLE_MOVESTACK
#define MOD_ENABLE_MOVESTACK 1
#endif
#ifndef MOD_ENABLE_PERTAG
#define MOD_ENABLE_PERTAG 1
#endif
#ifndef MOD_ENABLE_SCRATCHPAD
#define MOD_ENABLE_SCRATCHPAD 1
#endif
#ifndef MOD_ENABLE_STATUS2D
#define MOD_ENABLE_STATUS2D 1
#endif
#ifndef MOD_ENABLE_TITLESTATS
#define MOD_ENABLE_TITLESTATS 1
#endif

#if MOD_ENABLE_ALWAYSCENTER
#include "alwayscenter.hpp"
#endif
#if MOD_ENABLE_ATTACH_MODES
#include "attach_modes.hpp"
#endif
#if MOD_ENABLE_FAKEFULLSCREEN
#include "fakefullscreen.hpp"
#endif
#if MOD_ENABLE_GAPS
#include "gaps.hpp"
#endif
#if MOD_ENABLE_MOVESTACK
#include "movestack.hpp"
#endif
#if MOD_ENABLE_PERTAG
#include "pertag.hpp"
#endif
#if MOD_ENABLE_SCRATCHPAD
#include "scratchpad.hpp"
#endif
#if MOD_ENABLE_STATUS2D
#include "status2d.hpp"
#endif
#if MOD_ENABLE_TITLESTATS
#include "titlestats.hpp"
#endif
