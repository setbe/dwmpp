#pragma once

namespace mods {

struct FakeOfficialMod {
	template <typename MonitorT>
	inline static void before_arrange(MonitorT&) noexcept {}

	inline static void on_start() noexcept {}
	inline static void on_setup() noexcept {}
	template <typename ClientT>
	inline static void on_manage(ClientT&) noexcept {}
	template <typename ClientT>
	inline static void before_manage_rules(ClientT&) noexcept {}
	template <typename ClientT>
	inline static void after_manage(ClientT&) noexcept {}
	template <typename ClientT>
	inline static void on_unmanage(ClientT&) noexcept {}
	template <typename ClientT>
	inline static void on_focus(ClientT&) noexcept {}
	template <typename MonitorT>
	inline static void after_arrange(MonitorT&) noexcept {}
	template <typename MonitorT>
	inline static void before_layout(MonitorT&) noexcept {}
	template <typename ClientT, typename GeometryT>
	inline static void before_apply_geometry(ClientT&, const GeometryT&) noexcept {}
	template <typename ClientT>
	inline static void after_apply_geometry(ClientT&) noexcept {}
	template <typename ClientT, typename GeometryT>
	inline static void before_resize_client(ClientT&, const GeometryT&) noexcept {}
	template <typename ClientT>
	inline static void after_resize_client(ClientT&) noexcept {}
	template <typename MonitorT>
	inline static void on_draw_bar(MonitorT&) noexcept {}
	template <typename MonitorT>
	inline static void on_view(MonitorT&) noexcept {}
	template <typename MonitorT>
	inline static void on_set_layout(MonitorT&) noexcept {}
	template <typename MonitorT>
	inline static void on_set_mfact(MonitorT&) noexcept {}
	template <typename StatusT>
	inline static void on_update_status(const StatusT&) noexcept {}
	template <typename KeyEventT>
	inline static void on_key(const KeyEventT&) noexcept {}
	template <typename ButtonEventT>
	inline static void on_button(const ButtonEventT&, unsigned int) noexcept {}
	template <typename PropertyEventT, typename ClientT>
	inline static void on_property_notify(const PropertyEventT&, ClientT*) noexcept {}
	template <typename ConfigureRequestEventT, typename ClientT>
	inline static void on_configure_request(const ConfigureRequestEventT&, ClientT*) noexcept {}
};

} // namespace mods

template <>
struct CommandResolver<"test:ping"> {
	static consteval void (*resolve())(const Arg *) { return &spawn; }
};
