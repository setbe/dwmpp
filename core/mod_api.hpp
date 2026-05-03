#pragma once
#include <cctype>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

namespace core {

inline const char* user_data_dir() noexcept
{
	static char path[512];
	static bool init = false;
	if (!init) {
		const char *home = std::getenv("HOME");
		if (!home || !*home)
			home = ".";
		std::snprintf(path, sizeof(path), "%s/.dwmpp", home);
		mkdir(path, 0755);
		init = true;
	}
	return path;
}

inline bool core_file_path(const char *file_name, char *out, std::size_t out_sz) noexcept
{
	if (!file_name || !out || out_sz == 0)
		return false;
	if (std::strchr(file_name, '/') || std::strchr(file_name, '\\'))
		return false;
	int n = std::snprintf(out, out_sz, "%s/%s", user_data_dir(), file_name);
	return n > 0 && (std::size_t)n < out_sz;
}

inline const char* mods_workdir() noexcept
{
	static char path[512];
	static bool init = false;
	if (!init) {
		const char *base = user_data_dir();
		std::size_t blen = std::strlen(base);
		if (blen + 1 + 4 + 1 >= sizeof(path))
			return base;
		std::memcpy(path, base, blen);
		path[blen] = '/';
		path[blen + 1] = 'm';
		path[blen + 2] = 'o';
		path[blen + 3] = 'd';
		path[blen + 4] = 's';
		path[blen + 5] = '\0';
		mkdir(path, 0755);
		init = true;
	}
	return path;
}

inline const char* hud_log_path() noexcept
{
	static char path[512];
	static bool init = false;
	if (!init) {
		(void)core_file_path("hud.log", path, sizeof(path));
		init = true;
	}
	return path;
}
inline constexpr int general_binary_header_size = 400;
inline constexpr int general_binary_magic_size = 16;
static_assert(sizeof(std::uint32_t) == 4, "DWM++ binary header expects 32-bit version fields");

struct GeneralBinaryHeader {
	char magic[general_binary_magic_size];
	std::uint32_t version;
	std::uint32_t count;
	char reserved[general_binary_header_size - general_binary_magic_size - sizeof(std::uint32_t) - sizeof(std::uint32_t)];
};
static_assert(sizeof(GeneralBinaryHeader) == general_binary_header_size, "General binary header must be 400 bytes");

inline bool mod_file_path(const char *mod_name, const char *file_name, char *out, std::size_t out_sz) noexcept
{
	if (!mod_name || !file_name || !out || out_sz == 0)
		return false;
	if (std::strchr(file_name, '/') || std::strchr(file_name, '\\'))
		return false;
	for (const char *p = mod_name; *p; ++p) {
		if (!(std::isalnum((unsigned char)*p) || *p == '_' || *p == '-'))
			return false;
	}
	char moddir[512];
	int dn = std::snprintf(moddir, sizeof(moddir), "%s/%s", mods_workdir(), mod_name);
	if (dn <= 0 || (std::size_t)dn >= sizeof(moddir))
		return false;
	mkdir(moddir, 0755);
	int n = std::snprintf(out, out_sz, "%s/%s", moddir, file_name);
	return n > 0 && (std::size_t)n < out_sz;
}

void log_line(const char *line) noexcept;
void logf(const char *fmt, ...) noexcept;
void logv(const char *fmt, va_list ap) noexcept;

} // namespace core
