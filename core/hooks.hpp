#pragma once
#include <cstddef>

namespace core {

template <typename... Mods>
struct ModList {};

template <typename List>
struct Hooks;

template <typename... Mods>
struct Hooks<ModList<Mods...>> {
	inline static void on_start() noexcept { (Mods::on_start(), ...); }
	inline static void on_setup() noexcept { (Mods::on_setup(), ...); }
	inline static void on_shutdown() noexcept { (Mods::on_shutdown(), ...); }
	template <typename ClientT>
	inline static void on_manage(ClientT& c) noexcept { (Mods::on_manage(c), ...); }
	template <typename ClientT>
	inline static void before_manage_rules(ClientT& c) noexcept { (Mods::before_manage_rules(c), ...); }
	template <typename ClientT>
	inline static void after_manage(ClientT& c) noexcept { (Mods::after_manage(c), ...); }
	template <typename ClientT>
	inline static void on_unmanage(ClientT& c) noexcept { (Mods::on_unmanage(c), ...); }
	template <typename ClientT>
	inline static void on_focus(ClientT& c) noexcept { (Mods::on_focus(c), ...); }
	template <typename MonitorT>
	inline static void before_arrange(MonitorT& m) noexcept { (Mods::before_arrange(m), ...); }
	template <typename MonitorT>
	inline static void after_arrange(MonitorT& m) noexcept { (Mods::after_arrange(m), ...); }
	template <typename MonitorT>
	inline static void before_layout(MonitorT& m) noexcept { (Mods::before_layout(m), ...); }
	template <typename ClientT, typename GeometryT>
	inline static void before_apply_geometry(ClientT& c, const GeometryT& g) noexcept { (Mods::before_apply_geometry(c, g), ...); }
	template <typename ClientT>
	inline static void after_apply_geometry(ClientT& c) noexcept { (Mods::after_apply_geometry(c), ...); }
	template <typename ClientT, typename GeometryT>
	inline static void before_resize_client(ClientT& c, const GeometryT& g) noexcept { (Mods::before_resize_client(c, g), ...); }
	template <typename ClientT>
	inline static void after_resize_client(ClientT& c) noexcept { (Mods::after_resize_client(c), ...); }
	template <typename MonitorT>
	inline static void on_draw_bar(MonitorT& m) noexcept { (Mods::on_draw_bar(m), ...); }
	template <typename MonitorT>
	inline static void on_view(MonitorT& m) noexcept { (Mods::on_view(m), ...); }
	template <typename MonitorT>
	inline static void on_set_layout(MonitorT& m) noexcept { (Mods::on_set_layout(m), ...); }
	template <typename MonitorT>
	inline static void on_set_mfact(MonitorT& m) noexcept { (Mods::on_set_mfact(m), ...); }
	template <typename StatusT>
	inline static void on_update_status(const StatusT& s) noexcept { (Mods::on_update_status(s), ...); }
	template <typename KeyEventT>
	inline static void on_key(const KeyEventT& ev) noexcept { (Mods::on_key(ev), ...); }
	template <typename ButtonEventT>
	inline static void on_button(const ButtonEventT& ev, unsigned int click) noexcept { (Mods::on_button(ev, click), ...); }
	template <typename PropertyEventT, typename ClientT>
	inline static void on_property_notify(const PropertyEventT& ev, ClientT* c) noexcept { (Mods::on_property_notify(ev, c), ...); }
	template <typename ConfigureRequestEventT, typename ClientT>
	inline static void on_configure_request(const ConfigureRequestEventT& ev, ClientT* c) noexcept { (Mods::on_configure_request(ev, c), ...); }
	template <typename PaletteT>
	inline static void on_palette_changed(const PaletteT& p) noexcept { (Mods::on_palette_changed(p), ...); }
	template <typename ClientT>
	inline static void format_client_title(const ClientT& c, char *buf, std::size_t sz) noexcept { (Mods::format_client_title(c, buf, sz), ...); }
};

struct NoMod {
	inline static void on_start() noexcept {}
	inline static void on_setup() noexcept {}
	inline static void on_shutdown() noexcept {}
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
	inline static void before_arrange(MonitorT&) noexcept {}
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
	template <typename PaletteT>
	inline static void on_palette_changed(const PaletteT&) noexcept {}
	template <typename ClientT>
	inline static void format_client_title(const ClientT&, char*, std::size_t) noexcept {}
};

struct TestMod {
	inline static int arrange_counter = 0;
	template <typename MonitorT>
	inline static void before_arrange(MonitorT&) noexcept { ++arrange_counter; }
	inline static void on_start() noexcept {}
	inline static void on_setup() noexcept {}
	inline static void on_shutdown() noexcept {}
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
	template <typename PaletteT>
	inline static void on_palette_changed(const PaletteT&) noexcept {}
	template <typename ClientT>
	inline static void format_client_title(const ClientT&, char*, std::size_t) noexcept {}
};

using DefaultMods = ModList<>;
template <typename = void>
struct ActiveModsSelector {
	using type = DefaultMods;
};

} // namespace core
