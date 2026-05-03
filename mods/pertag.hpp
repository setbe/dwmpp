#pragma once

#include "mod_base.hpp"

namespace mods {
inline constexpr unsigned int pertag_tag_count = 9;

struct PertagState {
	Monitor *m = nullptr;
	unsigned int curtag = 1;
	unsigned int prevtag = 1;
	int nmasters[pertag_tag_count + 1]{};
	float mfacts[pertag_tag_count + 1]{};
	const Layout *lts[pertag_tag_count + 1]{};
};

inline PertagState& pertag_state_for(Monitor *m) noexcept
{
	static PertagState slots[16]{};
	for (auto &s : slots) if (s.m == m) return s;
	for (auto &s : slots) {
		if (!s.m) {
			s.m = m;
			s.curtag = s.prevtag = 1;
			for (unsigned int i = 0; i <= pertag_tag_count; ++i) {
				s.nmasters[i] = m->nmaster;
				s.mfacts[i] = m->mfact;
				s.lts[i] = m->lt[m->sellt];
			}
			return s;
		}
	}
	return slots[0];
}

inline unsigned int active_tag_index(Monitor *m) noexcept
{
	for (unsigned int i = 0; i < pertag_tag_count; ++i)
		if (m->tagset[m->seltags] & (1u << i))
			return i + 1;
	return 0;
}

inline void pertag_set_layout(const Arg *arg) noexcept
{
	setlayout(arg);
}

inline void pertag_set_mfact(const Arg *arg) noexcept
{
	setmfact(arg);
}

inline void pertag_inc_nmaster(const Arg *arg) noexcept
{
	if (!selmon || !arg)
		return;
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	PertagState &s = pertag_state_for(selmon);
	s.nmasters[s.curtag] = selmon->nmaster;
	arrange(selmon);
}

struct PertagMod : ModBase {
	template <typename MonitorT>
	inline static void on_view(MonitorT& m) noexcept
	{
		PertagState &s = pertag_state_for(&m);
		s.prevtag = s.curtag;
		s.curtag = active_tag_index(&m);
		m.nmaster = s.nmasters[s.curtag];
		m.mfact = s.mfacts[s.curtag];
		if (s.lts[s.curtag])
			m.lt[m.sellt] = s.lts[s.curtag];
	}

	template <typename MonitorT>
	inline static void on_set_layout(MonitorT& m) noexcept
	{
		PertagState &s = pertag_state_for(&m);
		s.lts[s.curtag] = m.lt[m.sellt];
	}

	template <typename MonitorT>
	inline static void on_set_mfact(MonitorT& m) noexcept
	{
		PertagState &s = pertag_state_for(&m);
		s.mfacts[s.curtag] = m.mfact;
	}
};

} // namespace mods

template <>
struct CommandResolver<"pertag:set_layout"> {
	static consteval void (*resolve())(const Arg *) { return &mods::pertag_set_layout; }
};
template <>
struct CommandResolver<"pertag:set_mfact"> {
	static consteval void (*resolve())(const Arg *) { return &mods::pertag_set_mfact; }
};
template <>
struct CommandResolver<"pertag:inc_nmaster"> {
	static consteval void (*resolve())(const Arg *) { return &mods::pertag_inc_nmaster; }
};
