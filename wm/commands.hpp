#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

template <size_t N>
struct FixedString {
	std::array<char, N> v{};
	consteval FixedString(const char (&s)[N]) noexcept
	{
		for (size_t i = 0; i < N; ++i)
			v[i] = s[i];
	}
};

template <typename>
struct AlwaysFalse : std::false_type {};

template <FixedString S>
struct CommandDescription {
	static_assert(AlwaysFalse<decltype(S)>::value, "Missing command description");
};

template <FixedString S>
consteval const char *command_desc()
{
	return CommandDescription<S>::value;
}

template <FixedString S>
consteval bool command_literal_valid() noexcept
{
	size_t colon = 0;
	for (size_t i = 0; i + 1 < S.v.size(); ++i) {
		const unsigned char ch = static_cast<unsigned char>(S.v[i]);
		if (ch < 0x21 || ch > 0x7e)
			return false;
		if (S.v[i] == ':') {
			if (i == 0 || i + 1 >= S.v.size() - 1)
				return false;
			++colon;
		}
	}
	return colon == 1;
}

template <FixedString S>
struct CommandResolver {
	static consteval void (*resolve())(const Arg *) {
		static_assert(AlwaysFalse<decltype(S)>::value, "Unknown command literal");
		return nullptr;
	}
};

template <FixedString S>
consteval void (*command_fn())(const Arg *)
{
	static_assert(command_literal_valid<S>(), "Command literal must be in format namespace:command");
	(void)command_desc<S>();
	return CommandResolver<S>::resolve();
}

template <>
struct CommandResolver<"core:spawn"> { static consteval void (*resolve())(const Arg *) { return &spawn; } };
template <>
struct CommandResolver<"core:toggle_bar"> { static consteval void (*resolve())(const Arg *) { return &togglebar; } };
template <>
struct CommandResolver<"core:focus_next"> { static consteval void (*resolve())(const Arg *) { return &core_focus_next_cmd; } };
template <>
struct CommandResolver<"core:focus_prev"> { static consteval void (*resolve())(const Arg *) { return &core_focus_prev_cmd; } };
template <>
struct CommandResolver<"core:inc_nmaster"> { static consteval void (*resolve())(const Arg *) { return &incnmaster; } };
template <>
struct CommandResolver<"core:set_mfact"> { static consteval void (*resolve())(const Arg *) { return &setmfact; } };
template <>
struct CommandResolver<"core:zoom"> { static consteval void (*resolve())(const Arg *) { return &zoom; } };
template <>
struct CommandResolver<"core:view"> { static consteval void (*resolve())(const Arg *) { return &view; } };
template <>
struct CommandResolver<"core:kill_client"> { static consteval void (*resolve())(const Arg *) { return &killclient; } };
template <>
struct CommandResolver<"core:set_layout"> { static consteval void (*resolve())(const Arg *) { return &setlayout; } };
template <>
struct CommandResolver<"core:toggle_floating"> { static consteval void (*resolve())(const Arg *) { return &togglefloating; } };
template <>
struct CommandResolver<"core:move_mouse"> { static consteval void (*resolve())(const Arg *) { return &movemouse; } };
template <>
struct CommandResolver<"core:resize_mouse"> { static consteval void (*resolve())(const Arg *) { return &resizemouse; } };
template <>
struct CommandResolver<"core:tag"> { static consteval void (*resolve())(const Arg *) { return &tag; } };
template <>
struct CommandResolver<"core:focusmon_prev"> { static consteval void (*resolve())(const Arg *) { return &focusmon; } };
template <>
struct CommandResolver<"core:focusmon_next"> { static consteval void (*resolve())(const Arg *) { return &focusmon; } };
template <>
struct CommandResolver<"core:tagmon_prev"> { static consteval void (*resolve())(const Arg *) { return &tagmon; } };
template <>
struct CommandResolver<"core:tagmon_next"> { static consteval void (*resolve())(const Arg *) { return &tagmon; } };
template <>
struct CommandResolver<"core:toggle_view"> { static consteval void (*resolve())(const Arg *) { return &toggleview; } };
template <>
struct CommandResolver<"core:toggle_tag"> { static consteval void (*resolve())(const Arg *) { return &toggletag; } };
template <>
struct CommandResolver<"core:quit"> { static consteval void (*resolve())(const Arg *) { return &quit; } };
template <>
struct CommandResolver<"core:toggle_hud"> { static consteval void (*resolve())(const Arg *) { return &togglehud; } };

template <> struct CommandDescription<"core:spawn"> { static constexpr const char *value = "Spawn process"; };
template <> struct CommandDescription<"core:toggle_bar"> { static constexpr const char *value = "Toggle top bar"; };
template <> struct CommandDescription<"core:focus_next"> { static constexpr const char *value = "Focus next client"; };
template <> struct CommandDescription<"core:focus_prev"> { static constexpr const char *value = "Focus previous client"; };
template <> struct CommandDescription<"core:inc_nmaster"> { static constexpr const char *value = "Change master count"; };
template <> struct CommandDescription<"core:set_mfact"> { static constexpr const char *value = "Adjust master factor"; };
template <> struct CommandDescription<"core:zoom"> { static constexpr const char *value = "Promote client to master"; };
template <> struct CommandDescription<"core:view"> { static constexpr const char *value = "View tag set"; };
template <> struct CommandDescription<"core:kill_client"> { static constexpr const char *value = "Kill focused client"; };
template <> struct CommandDescription<"core:set_layout"> { static constexpr const char *value = "Set layout"; };
template <> struct CommandDescription<"core:toggle_floating"> { static constexpr const char *value = "Toggle floating mode"; };
template <> struct CommandDescription<"core:move_mouse"> { static constexpr const char *value = "Move window by mouse"; };
template <> struct CommandDescription<"core:resize_mouse"> { static constexpr const char *value = "Resize window by mouse"; };
template <> struct CommandDescription<"core:tag"> { static constexpr const char *value = "Assign tags to client"; };
template <> struct CommandDescription<"core:focusmon_prev"> { static constexpr const char *value = "Focus previous monitor"; };
template <> struct CommandDescription<"core:focusmon_next"> { static constexpr const char *value = "Focus next monitor"; };
template <> struct CommandDescription<"core:tagmon_prev"> { static constexpr const char *value = "Send client to previous monitor"; };
template <> struct CommandDescription<"core:tagmon_next"> { static constexpr const char *value = "Send client to next monitor"; };
template <> struct CommandDescription<"core:toggle_view"> { static constexpr const char *value = "Toggle viewed tag"; };
template <> struct CommandDescription<"core:toggle_tag"> { static constexpr const char *value = "Toggle client tag"; };
template <> struct CommandDescription<"core:quit"> { static constexpr const char *value = "Quit window manager"; };
template <> struct CommandDescription<"core:toggle_hud"> { static constexpr const char *value = "Toggle hotkey HUD"; };
template <> struct CommandDescription<"gaps:increase"> { static constexpr const char *value = "Increase gaps"; };
template <> struct CommandDescription<"gaps:decrease"> { static constexpr const char *value = "Decrease gaps"; };
template <> struct CommandDescription<"gaps:toggle"> { static constexpr const char *value = "Toggle gaps"; };
template <> struct CommandDescription<"gaps:reset"> { static constexpr const char *value = "Reset gaps"; };
template <> struct CommandDescription<"movestack:down"> { static constexpr const char *value = "Move client down"; };
template <> struct CommandDescription<"movestack:up"> { static constexpr const char *value = "Move client up"; };
template <> struct CommandDescription<"scratchpad:toggle"> { static constexpr const char *value = "Toggle scratchpad"; };
template <> struct CommandDescription<"pertag:set_layout"> { static constexpr const char *value = "Set layout and store for current tag"; };
template <> struct CommandDescription<"pertag:set_mfact"> { static constexpr const char *value = "Set mfact and store for current tag"; };
template <> struct CommandDescription<"pertag:inc_nmaster"> { static constexpr const char *value = "Change nmaster and store for current tag"; };
template <> struct CommandDescription<"attach:head"> { static constexpr const char *value = "Attach at stack head"; };
template <> struct CommandDescription<"attach:aside"> { static constexpr const char *value = "Attach after selected"; };
template <> struct CommandDescription<"attach:bottom"> { static constexpr const char *value = "Attach at stack bottom"; };
template <> struct CommandDescription<"fakefullscreen:toggle"> { static constexpr const char *value = "Toggle fake fullscreen"; };
template <> struct CommandDescription<"fakefullscreen:toggle_global"> { static constexpr const char *value = "Toggle fake fullscreen mode"; };
template <> struct CommandDescription<"titlestats:toggle"> { static constexpr const char *value = "Toggle title stats overlay"; };
template <> struct CommandDescription<"test:ping"> { static constexpr const char *value = "Test command"; };

#if __has_include("../mods/commands_ext.hpp")
#include "../mods/commands_ext.hpp"
#endif

#if __has_include("../config.commands.hpp")
#include "../config.commands.hpp" /* backward compatibility */
#endif
