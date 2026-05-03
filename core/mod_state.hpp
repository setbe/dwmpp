#pragma once

namespace core {

template <typename Mod, typename StateT>
struct ModState {
	inline static StateT value{};
};

} // namespace core
