#pragma once

#include "mod_base.hpp"

namespace mods {

inline void movestack(const Arg *arg) noexcept
{
	Client *c = nullptr, *p = nullptr, *pc = nullptr, *i;
	Client *sel = selmon ? selmon->sel : nullptr;
	if (!sel)
		return;

	if (arg && arg->i > 0) {
		for (c = sel->next; c && c->isfloating; c = c->next) {}
		if (!c)
			return;
		for (i = selmon->clients; i && i != sel; i = i->next)
			if (!i->isfloating)
				p = i;
		for (i = selmon->clients; i && i != c; i = i->next)
			if (!i->isfloating)
				pc = i;

		if (p)
			p->next = c;
		else
			selmon->clients = c;
		if (pc)
			pc->next = sel;
		else
			selmon->clients = sel;

		sel->next = c->next;
		c->next = sel;
	} else {
		for (i = selmon->clients; i && i != sel; i = i->next)
			if (!i->isfloating) {
				pc = p;
				p = i;
			}
		if (!p)
			return;

		if (pc)
			pc->next = sel;
		else
			selmon->clients = sel;

		if (p->next == sel)
			p->next = sel->next;
		sel->next = p;
	}

	arrange(selmon);
}

struct MoveStackMod : ModBase {};

} // namespace mods

template <>
struct CommandResolver<"movestack:down"> {
	static consteval void (*resolve())(const Arg *) { return &mods::movestack; }
};
template <>
struct CommandResolver<"movestack:up"> {
	static consteval void (*resolve())(const Arg *) { return &mods::movestack; }
};
