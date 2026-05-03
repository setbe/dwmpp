#include <array>
#include <cstddef>
#include <cstdint>

using CommandArg = int;
using CommandFn = void (*)(CommandArg) noexcept;

namespace core {
static void focus_next(CommandArg) noexcept {}
}

namespace mods::gaps {
static void increase(CommandArg) noexcept {}
}

template <size_t N>
struct FixedString {
	std::array<char, N> v{};
	consteval FixedString(const char (&s)[N]) noexcept {
		for (size_t i = 0; i < N; ++i) v[i] = s[i];
	}
};

template <typename>
struct AlwaysFalse : std::false_type {};

template <FixedString Name>
struct CommandResolver {
	static consteval CommandFn resolve() noexcept {
		static_assert(AlwaysFalse<decltype(Name)>::value, "unknown command");
		return nullptr;
	}
};

template <>
struct CommandResolver<"core:focus_next"> {
	static consteval CommandFn resolve() noexcept { return &core::focus_next; }
};

template <>
struct CommandResolver<"gaps:increase"> {
	static consteval CommandFn resolve() noexcept { return &mods::gaps::increase; }
};

template <FixedString Name>
consteval CommandFn resolve_command() noexcept {
	return CommandResolver<Name>::resolve();
}

static_assert(resolve_command<"core:focus_next">() == &core::focus_next);
static_assert(resolve_command<"gaps:increase">() == &mods::gaps::increase);

void test_call() noexcept {
	constexpr CommandFn fn = resolve_command<"gaps:increase">();
	fn(0);
}
