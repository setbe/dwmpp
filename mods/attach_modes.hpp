#pragma once

#include "mod_base.hpp"

namespace mods {

enum class AttachMode : int {
	Head = 0,
	Aside = 1,
	Bottom = 2,
};

inline AttachMode& attach_mode_state() noexcept
{
	static AttachMode mode = AttachMode::Head;
	return mode;
}

inline void attachmode_head(const Arg *) noexcept { attach_mode_state() = AttachMode::Head; }
inline void attachmode_aside(const Arg *) noexcept { attach_mode_state() = AttachMode::Aside; }
inline void attachmode_bottom(const Arg *) noexcept { attach_mode_state() = AttachMode::Bottom; }

inline void attach_client(Client *c) noexcept
{
	if (!c || !c->mon) return;
	Client *sel = c->mon->sel;
	Client **head = &c->mon->clients;

	switch (attach_mode_state()) {
	case AttachMode::Aside:
		if (!sel || sel->isfloating) {
			c->next = *head;
			*head = c;
			return;
		}
		c->next = sel->next;
		sel->next = c;
		return;
	case AttachMode::Bottom: {
		Client *it = *head;
		if (!it) {
			c->next = nullptr;
			*head = c;
			return;
		}
		while (it->next) it = it->next;
		it->next = c;
		c->next = nullptr;
		return;
	}
	case AttachMode::Head:
	default:
		c->next = *head;
		*head = c;
		return;
	}
}

struct AttachModesMod : ModBase {};

} // namespace mods

template <>
struct CommandResolver<"attach:head"> { static consteval void (*resolve())(const Arg *) { return &mods::attachmode_head; } };
template <>
struct CommandResolver<"attach:aside"> { static consteval void (*resolve())(const Arg *) { return &mods::attachmode_aside; } };
template <>
struct CommandResolver<"attach:bottom"> { static consteval void (*resolve())(const Arg *) { return &mods::attachmode_bottom; } };
