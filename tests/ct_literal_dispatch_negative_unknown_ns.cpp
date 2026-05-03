#include "ct_literal_dispatch_test.cpp"
constexpr auto kBadNs = resolve_command<"unknown:toggle">();
