#pragma once

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "mod_base.hpp"
#include "../core/mod_api.hpp"

namespace mods {

struct TitleStatsCache {
	int enabled = 1;
	pid_t pid = -1;
	unsigned long long last_proc_ticks = 0;
	unsigned long long last_wall_ns = 0;
	unsigned long long last_refresh_ns = 0;
	unsigned int cpu_percent = 0;
	unsigned long ram_mb = 0;
	int gpu_valid = 0;
	unsigned int gpu_percent = 0;
	unsigned long vram_mb = 0;
};

inline const char* titlestats_state_path() noexcept
{
	static char path[256];
	if (!core::mod_file_path("titlestats", "titlestats.bin", path, sizeof(path)))
		return nullptr;
	return path;
}

inline TitleStatsCache& titlestats_cache() noexcept
{
	static TitleStatsCache st{};
	return st;
}

inline unsigned long long now_ns() noexcept
{
	struct timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

inline void ensure_mods_dir() noexcept
{
	(void)core::mods_workdir();
	char p[256];
	(void)core::mod_file_path("titlestats", "titlestats.bin", p, sizeof(p));
}

inline void save_titlestats_state() noexcept
{
	ensure_mods_dir();
	const char *path = titlestats_state_path();
	if (!path) return;
	FILE *f = fopen(path, "wb");
	if (!f) return;
	core::GeneralBinaryHeader h{};
	snprintf(h.magic, sizeof(h.magic), "TITLSTAT");
	h.version = 1u;
	h.count = 1u;
	unsigned int enabled = titlestats_cache().enabled ? 1u : 0u;
	fwrite(&h, sizeof(h), 1, f);
	fwrite(&enabled, sizeof(enabled), 1, f);
	fclose(f);
	core::logf("titlestats: state saved enabled=%u", enabled);
}

inline void load_titlestats_state() noexcept
{
	titlestats_cache().enabled = 1;
	const char *path = titlestats_state_path();
	if (!path) return;
	FILE *f = fopen(path, "rb");
	if (!f) {
		save_titlestats_state();
		core::log_line("titlestats: state file created");
		return;
	}
	core::GeneralBinaryHeader h{};
	unsigned int enabled = 1;
	if (fread(&h, sizeof(h), 1, f) == 1 &&
		fread(&enabled, sizeof(enabled), 1, f) == 1 &&
		strncmp(h.magic, "TITLSTAT", 8) == 0 && h.version == 1u) {
		titlestats_cache().enabled = enabled ? 1 : 0;
	}
	fclose(f);
	core::logf("titlestats: state loaded enabled=%d", titlestats_cache().enabled);
}

inline pid_t client_pid(const Client *c) noexcept
{
	if (!c) return -1;
	Atom atom = XInternAtom(dpy, "_NET_WM_PID", False);
	Atom actual = None;
	int format = 0;
	unsigned long nitems = 0, bytes = 0;
	unsigned char *prop = nullptr;
	pid_t out = -1;
	if (XGetWindowProperty(dpy, c->win, atom, 0L, 1L, False, XA_CARDINAL,
		&actual, &format, &nitems, &bytes, &prop) == Success && prop) {
		if (actual == XA_CARDINAL && format == 32 && nitems == 1)
			out = (pid_t)(*(unsigned long*)prop);
		XFree(prop);
	}
	if (out > 0)
		return out;

	/* fallback: check WM_CLIENT_LEADER window for _NET_WM_PID */
	Atom leader_atom = XInternAtom(dpy, "WM_CLIENT_LEADER", False);
	Window leader = 0;
	actual = None;
	format = 0;
	nitems = 0;
	bytes = 0;
	prop = nullptr;
	if (XGetWindowProperty(dpy, c->win, leader_atom, 0L, 1L, False, XA_WINDOW,
		&actual, &format, &nitems, &bytes, &prop) == Success && prop) {
		if (actual == XA_WINDOW && format == 32 && nitems == 1)
			leader = (Window)(*(unsigned long*)prop);
		XFree(prop);
	}
	if (!leader)
		return -1;
	prop = nullptr;
	if (XGetWindowProperty(dpy, leader, atom, 0L, 1L, False, XA_CARDINAL,
		&actual, &format, &nitems, &bytes, &prop) == Success && prop) {
		if (actual == XA_CARDINAL && format == 32 && nitems == 1)
			out = (pid_t)(*(unsigned long*)prop);
		XFree(prop);
	}
	return out;
}

inline int read_proc_ticks(pid_t pid, unsigned long long *ticks) noexcept
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	char line[2048];
	if (!fgets(line, sizeof(line), f)) {
		fclose(f);
		return 0;
	}
	fclose(f);
	char *rparen = strrchr(line, ')');
	if (!rparen) return 0;
	char *p = rparen + 2;
	int field = 3;
	unsigned long long utime = 0, stime = 0;
	while (field <= 15 && *p) {
		while (*p == ' ') ++p;
		char *end = p;
		while (*end && *end != ' ') ++end;
		char saved = *end;
		*end = '\0';
		if (field == 14) utime = strtoull(p, nullptr, 10);
		if (field == 15) stime = strtoull(p, nullptr, 10);
		*end = saved;
		p = end;
		++field;
	}
	*ticks = utime + stime;
	return 1;
}

inline int read_proc_rss_mb(pid_t pid, unsigned long *mb) noexcept
{
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "VmRSS:", 6) == 0) {
			unsigned long kb = strtoul(line + 6, nullptr, 10);
			*mb = kb / 1024ul;
			fclose(f);
			return 1;
		}
	}
	fclose(f);
	/* fallback: /proc/<pid>/statm */
	snprintf(path, sizeof(path), "/proc/%d/statm", (int)pid);
	f = fopen(path, "r");
	if (!f)
		return 0;
	unsigned long size_pages = 0, rss_pages = 0;
	int ok = 0;
	if (fscanf(f, "%lu %lu", &size_pages, &rss_pages) == 2) {
		long ps = sysconf(_SC_PAGESIZE);
		if (ps > 0) {
			*mb = (rss_pages * (unsigned long)ps) / (1024ul * 1024ul);
			ok = 1;
		}
	}
	fclose(f);
	return ok;
}

inline int query_nvidia_gpu_and_vram(pid_t pid, unsigned int *gpu_percent, unsigned long *vram_mb) noexcept
{
	FILE *fp = popen("nvidia-smi 2>/dev/null", "r");
	if (!fp) return 0;
	char line[1024];
	int have_gpu = 0;
	int have_vram = 0;
	unsigned int global_gpu = 0;
	unsigned long proc_vram = 0;
	int in_proc_table = 0;
	while (fgets(line, sizeof(line), fp)) {
		/* parse global GPU-Util line, e.g. "... |      0%      Default |" */
		if (!have_gpu && strstr(line, "%") && strstr(line, "Default")) {
			char *pct = strchr(line, '%');
			if (pct) {
				char *s = pct;
				while (s > line && isdigit((unsigned char)*(s - 1)))
					--s;
				unsigned int val = (unsigned int)strtoul(s, nullptr, 10);
				global_gpu = val;
				have_gpu = 1;
			}
		}
		if (strstr(line, "| Processes:"))
			in_proc_table = 1;
		if (!in_proc_table)
			continue;
		/* parse process lines from the standard nvidia-smi table */
		if (strstr(line, "MiB |")) {
			int p = -1;
			unsigned long mem = 0;
			if (sscanf(line, " | %*d %*s %*s %d %*s %*[^|]| %luMiB |", &p, &mem) == 2 && p == (int)pid) {
				proc_vram = mem;
				have_vram = 1;
			}
		}
	}
	pclose(fp);
	if (have_gpu && have_vram) {
		*gpu_percent = global_gpu;
		*vram_mb = proc_vram;
		return 1;
	}
	return 0;
}

inline void refresh_titlestats(const Client *c) noexcept
{
	TitleStatsCache &st = titlestats_cache();
	if (!st.enabled || !c)
		return;
	pid_t pid = client_pid(c);
	if (pid <= 0) {
		st.pid = -1;
		return;
	}
	unsigned long long now = now_ns();
	if (st.pid == pid && st.last_refresh_ns != 0 && now - st.last_refresh_ns < 4000000000ull)
		return;

	unsigned long long ticks = 0;
	unsigned long rss_mb = 0;
	if (!read_proc_ticks(pid, &ticks) || !read_proc_rss_mb(pid, &rss_mb)) {
		st.pid = pid;
		st.gpu_valid = 0;
		st.ram_mb = 0;
		st.cpu_percent = 0;
		st.last_refresh_ns = now;
		return;
	}

	if (st.pid == pid && st.last_wall_ns > 0 && now > st.last_wall_ns && ticks >= st.last_proc_ticks) {
		double dt = (double)(now - st.last_wall_ns) / 1e9;
		double proc = (double)(ticks - st.last_proc_ticks) / (double)sysconf(_SC_CLK_TCK);
		double pct = dt > 0.0 ? (proc / dt) * 100.0 : 0.0;
		if (pct < 0.0) pct = 0.0;
		if (pct > 999.0) pct = 999.0;
		st.cpu_percent = (unsigned int)pct;
	} else {
		st.cpu_percent = 0;
	}
	st.ram_mb = rss_mb;
	st.pid = pid;
	st.last_proc_ticks = ticks;
	st.last_wall_ns = now;
	st.last_refresh_ns = now;

	st.gpu_valid = 0;
	unsigned long vram = 0;
	unsigned int gpu = 0;
	if (query_nvidia_gpu_and_vram(pid, &gpu, &vram)) {
		st.gpu_percent = gpu;
		st.vram_mb = vram;
		st.gpu_valid = 1;
	}
}

inline void titlestats_toggle(const Arg *) noexcept
{
	TitleStatsCache &st = titlestats_cache();
	st.enabled = !st.enabled;
	save_titlestats_state();
	core::logf("titlestats: toggled enabled=%d", st.enabled);
	drawbars();
}

struct TitleStatsMod : ModBase {
	inline static void on_setup() noexcept { load_titlestats_state(); }
	inline static void on_shutdown() noexcept { save_titlestats_state(); }
	template <typename ClientT>
	inline static void format_client_title(const ClientT& c, char *buf, std::size_t sz) noexcept
	{
		TitleStatsCache &st = titlestats_cache();
		if (!st.enabled || !buf || sz == 0)
			return;
		refresh_titlestats(&c);
		if (st.gpu_valid)
			snprintf(buf, sz, "%s [CPU: %u%% %lu MB | GPU: %u%% %lu MB]", c.name, st.cpu_percent, st.ram_mb, st.gpu_percent, st.vram_mb);
		else
			snprintf(buf, sz, "%s [%u%% %lu MB]", c.name, st.cpu_percent, st.ram_mb);
	}
};

} // namespace mods

template <>
struct CommandResolver<"titlestats:toggle"> {
	static consteval void (*resolve())(const Arg *) { return &mods::titlestats_toggle; }
};
