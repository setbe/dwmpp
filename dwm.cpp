/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <cstddef>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "core/hooks.hpp"
#include "core/log.hpp"
#include "core/mod_api.hpp"
#include "core/palette.hpp"
#include "core/types.hpp"
#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	Client *next;
	Client *snext;
	Monitor *mon;
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
	const char *cmd;
	const char *desc;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	Window hudwin;
	const Layout *lt[2];
};

static_assert(std::is_trivially_copyable<Client>::value, "Client must stay trivially copyable");
static_assert(std::is_trivially_copyable<Monitor>::value, "Monitor must stay trivially copyable");

typedef struct {
	const char *class_;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
} Rule;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m) noexcept;
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c) noexcept;
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa) noexcept;
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact) noexcept;
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void) noexcept;
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void) noexcept;
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void spawn(const Arg *arg);
static void apply_palette_to_scheme(void);
static void load_runtime_palette(void);
static void save_runtime_palette(void);
static void load_hud_log(void);
static void save_hud_log(void);
static bool palette_try_set_with_guard(core::ColorId id, core::Color c, char *warn, size_t warnsz);
static void update_hud_keygrabs(void);
static void grab_hud_keyboard(void);
static void ungrab_hud_keyboard(void);
static void init_runtime_key_bindings(void);
static void load_runtime_key_bindings(void);
static void save_runtime_key_bindings(void);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m) noexcept;
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglehud(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static void init_handlers(void);
static void core_focus_next_cmd(const Arg *) noexcept;
static void core_focus_prev_cmd(const Arg *) noexcept;
static void apply_client_geometry(Client *c, int x, int y, int w, int h, int interact);
static void layout_dispatch(Monitor *m);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

/* variables */
static const char broken[] = "broken";
static int (*xerrorxlib)(Display *, XErrorEvent *);
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height */
static int lrpad;            /* sum of left and right padding for text */
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *);
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
core::Palette g_palette;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
static int showhud = 0;
static int hud_keyboard_grabbed = 0;
static int hud_selected_row = 0;
static int hud_scroll_top = 0;
static int hud_edit_armed = 0;
static int hud_edit_capture = 0;
enum { HudPageKeys = 0, HudPagePalette = 1, HudPageLog = 2 };
static int hud_page = HudPageKeys;
enum { HudCaptureNone = 0, HudCaptureMod = 1, HudCaptureKey = 2 };
enum { PaletteChannelR = 0, PaletteChannelG = 1, PaletteChannelB = 2, PaletteChannelA = 3 };
static int palette_selected_row = 0;
static int palette_channel = PaletteChannelR;
static int hud_log_scroll = 0;
static char hud_warn[256] = {0};
static core::Color palette_draft[core::color_count]{};
static int palette_dirty = 0;

struct KeyBindingRecord {
	uint64_t modifiers;
	uint64_t key;
	char command[128];
	char description[256];
};
static_assert(sizeof(KeyBindingRecord) == 400, "KeyBindingRecord must be exactly 400 bytes");
static core::HudLogBuffer g_hud_log;

namespace core {

void log_line(const char *line) noexcept
{
	g_hud_log.append_line(line);
}

void logv(const char *fmt, va_list ap) noexcept
{
	char buf[512];
	vsnprintf(buf, sizeof(buf), fmt, ap);
	g_hud_log.append_line(buf);
}

void logf(const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	logv(fmt, ap);
	va_end(ap);
}

} // namespace core
struct WMStateView {
	char (*stext)[256];
	int *screen;
	int *sw;
	int *sh;
	int *bh;
	int *lrpad;
	unsigned int *numlockmask;
	Atom (*wmatom)[WMLast];
	Atom (*netatom)[NetLast];
	int *running;
	Monitor **mons;
	Monitor **selmon;
};

static inline WMStateView wm_state_view(void) noexcept
{
	return { &stext, &screen, &sw, &sh, &bh, &lrpad, &numlockmask, &wmatom, &netatom, &running, &mons, &selmon };
}

static void core_focus_next_cmd(const Arg *) noexcept { Arg a = {0}; a.i = +1; focusstack(&a); }
static void core_focus_prev_cmd(const Arg *) noexcept { Arg a = {0}; a.i = -1; focusstack(&a); }
#include "wm/commands.hpp"

/* configuration, allows nested code to access above variables */
#include "config.hpp"
using Hooks = core::Hooks<typename core::ActiveModsSelector<>::type>;
struct RuntimeKey {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	Arg arg;
	const char *cmd;
	const char *desc;
};
static RuntimeKey keys[LENGTH(default_keys)];

struct DuplicateBindingInfo {
	bool found;
	unsigned mod;
	unsigned long sym;
	size_t i;
	size_t j;
	void (*func_a)(const Arg *);
	void (*func_b)(const Arg *);
};

consteval DuplicateBindingInfo find_first_duplicate_keybinding() noexcept
{
	for (size_t i = 0; i < LENGTH(default_keys); ++i) {
		for (size_t j = i + 1; j < LENGTH(default_keys); ++j) {
			if (default_keys[i].keysym == default_keys[j].keysym && default_keys[i].mod == default_keys[j].mod)
				return {
					true,
					default_keys[i].mod,
					static_cast<unsigned long>(default_keys[i].keysym),
					i,
					j,
					default_keys[i].func,
					default_keys[j].func
				};
		}
	}
	return {false, 0u, 0ul, 0u, 0u, nullptr, nullptr};
}

template <bool Found, unsigned Mod, unsigned long Sym, size_t I, size_t J, auto FuncA, auto FuncB>
struct DuplicateBindingReporter {
	static constexpr bool ok = true;
};

template <unsigned Mod, unsigned long Sym, size_t I, size_t J, auto FuncA, auto FuncB>
struct DuplicateBindingReporter<true, Mod, Sym, I, J, FuncA, FuncB> {
	static_assert(
		Mod == static_cast<unsigned>(-1),
		"Duplicate key binding detected in keys[]. "
		"See DuplicateBindingReporter<true, Mod, Sym, I, J, FuncA, FuncB> for exact combo, indices, and conflicting functions."
	);
	static constexpr bool ok = false;
};

constexpr DuplicateBindingInfo kDuplicateBinding = find_first_duplicate_keybinding();
static_assert(
	DuplicateBindingReporter<
		kDuplicateBinding.found,
		kDuplicateBinding.mod,
		kDuplicateBinding.sym,
		kDuplicateBinding.i,
		kDuplicateBinding.j,
		kDuplicateBinding.func_a,
		kDuplicateBinding.func_b
	>::ok,
	"Duplicate key binding detected in keys[]; reassign conflicting bindings in config.h."
);

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

void
init_runtime_key_bindings(void)
{
	for (unsigned int i = 0; i < LENGTH(default_keys); ++i)
		keys[i] = { default_keys[i].mod, default_keys[i].keysym, default_keys[i].func, default_keys[i].arg, default_keys[i].cmd, default_keys[i].desc };
}

void
save_runtime_key_bindings(void)
{
	char path[512];
	if (!core::core_file_path("key_bindings.bin", path, sizeof(path)))
		return;
	FILE *f = fopen(path, "wb");
	if (!f)
		return;
	core::GeneralBinaryHeader h{};
	snprintf(h.magic, sizeof(h.magic), "DWMKBIN");
	h.version = 1u;
	h.count = static_cast<std::uint32_t>(LENGTH(keys));
	if (fwrite(&h, sizeof(h), 1, f) != 1) {
		fclose(f);
		return;
	}
	for (unsigned int i = 0; i < LENGTH(keys); ++i) {
		KeyBindingRecord rec{};
		rec.modifiers = keys[i].mod;
		rec.key = static_cast<uint64_t>(keys[i].keysym);
		snprintf(rec.command, sizeof(rec.command), "%s", keys[i].cmd ? keys[i].cmd : "");
		snprintf(rec.description, sizeof(rec.description), "%s", keys[i].desc ? keys[i].desc : "");
		if (fwrite(&rec, sizeof(rec), 1, f) != 1)
			break;
	}
	fclose(f);
}

void
load_runtime_key_bindings(void)
{
	char path[512];
	if (!core::core_file_path("key_bindings.bin", path, sizeof(path)))
		return;
	FILE *f = fopen(path, "rb");
	if (!f) {
		save_runtime_key_bindings();
		return;
	}
	core::GeneralBinaryHeader h{};
	if (fread(&h, sizeof(h), 1, f) != 1 || strncmp(h.magic, "DWMKBIN", 7) != 0 || h.version != 1u) {
		fclose(f);
		save_runtime_key_bindings();
		return;
	}
	KeyBindingRecord rec{};
	while (fread(&rec, sizeof(rec), 1, f) == 1) {
		rec.command[sizeof(rec.command) - 1] = '\0';
		for (unsigned int i = 0; i < LENGTH(keys); ++i) {
			if (!keys[i].cmd || strcmp(keys[i].cmd, rec.command) != 0)
				continue;
			keys[i].mod = static_cast<unsigned int>(rec.modifiers);
			keys[i].keysym = static_cast<KeySym>(rec.key);
			break;
		}
	}
	fclose(f);
}

static const char *color_name(core::ColorId id) noexcept
{
	switch (id) {
	case core::ColorId::Bg: return "Bg";
	case core::ColorId::Fg: return "Fg";
	case core::ColorId::Border: return "Border";
	case core::ColorId::Accent: return "Accent";
	case core::ColorId::Urgent: return "Urgent";
	case core::ColorId::TagActive: return "TagActive";
	case core::ColorId::TagInactive: return "TagInactive";
	case core::ColorId::StatusFg: return "StatusFg";
	case core::ColorId::StatusBg: return "StatusBg";
	case core::ColorId::COUNT: break;
	}
	return "?";
}

void
apply_palette_to_scheme(void)
{
	if (!scheme)
		return;
	const core::Color bg = g_palette.get(core::ColorId::Bg);
	const core::Color fg = g_palette.get(core::ColorId::Fg);
	const core::Color border = g_palette.get(core::ColorId::Border);
	const core::Color accent = g_palette.get(core::ColorId::Accent);
	const core::Color tag_active = g_palette.get(core::ColorId::TagActive);
	drw_clr_create_rgba(drw, &scheme[SchemeNorm][ColBg], DrwColorF{bg.r, bg.g, bg.b, bg.a});
	drw_clr_create_rgba(drw, &scheme[SchemeNorm][ColFg], DrwColorF{fg.r, fg.g, fg.b, fg.a});
	drw_clr_create_rgba(drw, &scheme[SchemeNorm][ColBorder], DrwColorF{border.r, border.g, border.b, border.a});
	drw_clr_create_rgba(drw, &scheme[SchemeSel][ColBg], DrwColorF{accent.r, accent.g, accent.b, accent.a});
	drw_clr_create_rgba(drw, &scheme[SchemeSel][ColFg], DrwColorF{tag_active.r, tag_active.g, tag_active.b, tag_active.a});
	drw_clr_create_rgba(drw, &scheme[SchemeSel][ColBorder], DrwColorF{accent.r, accent.g, accent.b, accent.a});
}

bool
palette_try_set_with_guard(core::ColorId id, core::Color c, char *warn, size_t warnsz)
{
	c = core::color_clamp(c);
	core::Color bg = g_palette.get(core::ColorId::Bg);
	core::Color statusbg = g_palette.get(core::ColorId::StatusBg);
	if (id == core::ColorId::Bg) {
		const core::Color fg = g_palette.get(core::ColorId::Fg);
		if (!core::is_readable(fg, c)) {
			snprintf(warn, warnsz, "contrast too low; change colors gradually");
			return false;
		}
	}
	if (id == core::ColorId::StatusBg) {
		const core::Color sfg = g_palette.get(core::ColorId::StatusFg);
		if (!core::is_readable(sfg, c)) {
			snprintf(warn, warnsz, "contrast too low; change colors gradually");
			return false;
		}
	}
	if (id == core::ColorId::Fg || id == core::ColorId::TagActive || id == core::ColorId::TagInactive) {
		if (!core::is_readable(c, bg)) {
			snprintf(warn, warnsz, "contrast too low; change colors gradually");
			return false;
		}
	}
	if (id == core::ColorId::StatusFg) {
		if (!core::is_readable(c, statusbg)) {
			snprintf(warn, warnsz, "contrast too low; change colors gradually");
			return false;
		}
	}
	g_palette.try_set(id, c);
	g_palette.invalidate();
	Hooks::on_palette_changed(g_palette);
	apply_palette_to_scheme();
	if (warnsz > 0)
		warn[0] = '\0';
	return true;
}

void
save_runtime_palette(void)
{
	char path[512];
	char tmppath[512];
	if (!core::core_file_path("color_scheme.bin", path, sizeof(path)))
		return;
	if (std::snprintf(tmppath, sizeof(tmppath), "%s.tmp", path) <= 0)
		return;
	FILE *f = fopen(tmppath, "wb");
	if (!f)
		return;
	core::GeneralBinaryHeader h{};
	snprintf(h.magic, sizeof(h.magic), "DWMCOLR");
	h.version = 1u;
	h.count = static_cast<std::uint32_t>(core::color_count);
	if (fwrite(&h, sizeof(h), 1, f) != 1) {
		fclose(f);
		return;
	}
	for (std::size_t i = 0; i < core::color_count; ++i) {
		const core::Color c = g_palette.get(static_cast<core::ColorId>(i));
		float v[4] = {c.r, c.g, c.b, c.a};
		if (fwrite(v, sizeof(v), 1, f) != 1) {
			fclose(f);
			return;
		}
	}
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	rename(tmppath, path);
}

void
load_runtime_palette(void)
{
	g_palette.reset(core::default_palette);
	char path[512];
	if (!core::core_file_path("color_scheme.bin", path, sizeof(path)))
		return;
	FILE *f = fopen(path, "rb");
	if (!f) {
		save_runtime_palette();
		return;
	}
	core::GeneralBinaryHeader h{};
	if (fread(&h, sizeof(h), 1, f) != 1) {
		fclose(f);
		return;
	}
	if (strncmp(h.magic, "DWMCOLR", 7) != 0 || h.version != 1u || h.count != core::color_count) {
		fclose(f);
		return;
	}
	for (std::size_t i = 0; i < core::color_count; ++i) {
		float v[4] = {0.f, 0.f, 0.f, 1.f};
		if (fread(v, sizeof(v), 1, f) != 1)
			break;
		core::Color c = core::color_clamp(core::Color{v[0], v[1], v[2], v[3]});
		(void)palette_try_set_with_guard(static_cast<core::ColorId>(i), c, hud_warn, sizeof(hud_warn));
	}
	fclose(f);
}

void
load_hud_log(void)
{
	FILE *f = fopen(core::hud_log_path(), "rb");
	if (!f) {
		f = fopen(core::hud_log_path(), "wb");
		if (f) fclose(f);
		return;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	if (sz < 0) {
		fclose(f);
		return;
	}
	fseek(f, 0, SEEK_SET);
	std::size_t n = static_cast<std::size_t>(sz);
	if (n > core::hud_log_capacity)
		n = core::hud_log_capacity;
	static char buf[core::hud_log_capacity];
	std::size_t rd = (n > 0) ? fread(buf, 1, n, f) : 0;
	fclose(f);
	g_hud_log.load_snapshot(buf, rd);
}

void
save_hud_log(void)
{
	FILE *f = fopen(core::hud_log_path(), "wb");
	if (!f)
		return;
	static char buf[core::hud_log_capacity];
	const std::size_t n = g_hud_log.copy_snapshot(buf, sizeof(buf));
	if (n > 0)
		fwrite(buf, 1, n, f);
	fclose(f);
}

/* function implementations */
void
applyrules(Client *c)
{
	const char *class_name, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class_name = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class_ || strstr(class_name, r->class_))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		if (!c->hintsvalid)
			updatesizehints(c);
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m) noexcept
{
	Monitor *it;
	if (m) {
		Hooks::before_arrange(*m);
		showhide(m->stack);
		layout_dispatch(m);
		restack(m);
		Hooks::after_arrange(*m);
		return;
	}
	for (it = mons; it; it = it->next)
		showhide(it->stack);
	for (it = mons; it; it = it->next) {
		Hooks::before_arrange(*it);
		layout_dispatch(it);
		Hooks::after_arrange(*it);
	}
}

void
layout_dispatch(Monitor *m)
{
	Hooks::before_layout(*m);
	arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c)
{
	mods::attach_client(c);
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
	unsigned int i, x, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + TEXTW(selmon->ltsymbol))
			click = ClkLtSymbol;
		else if (ev->x > selmon->ww - (int)TEXTW(stext) + lrpad - 2)
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	Hooks::on_button(*ev, click);
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
init_handlers(void)
{
	for (int i = 0; i < LASTEvent; ++i)
		handler[i] = NULL;
	handler[ButtonPress] = buttonpress;
	handler[ClientMessage] = clientmessage;
	handler[ConfigureRequest] = configurerequest;
	handler[ConfigureNotify] = configurenotify;
	handler[DestroyNotify] = destroynotify;
	handler[EnterNotify] = enternotify;
	handler[Expose] = expose;
	handler[FocusIn] = focusin;
	handler[KeyPress] = keypress;
	handler[MappingNotify] = mappingnotify;
	handler[MapRequest] = maprequest;
	handler[MotionNotify] = motionnotify;
	handler[PropertyNotify] = propertynotify;
	handler[UnmapNotify] = unmapnotify;
}

void
cleanup(void)
{
	Arg a = {0};
	a.ui = ~0u;
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	ungrab_hud_keyboard();
	core::log_line("shutdown: begin cleanup");
	Hooks::on_shutdown();
	save_hud_log();
	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < 2; i++)
		drw_scm_free(drw, scheme[i], 3);
	/* arena allocator keeps memory until process exit */
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	if (mon->hudwin) {
		XUnmapWindow(dpy, mon->hudwin);
		XDestroyWindow(dpy, mon->hudwin);
	}
	XDestroyWindow(dpy, mon->barwin);
	/* arena allocator keeps memory until process exit */
}

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	c = wintoclient(ev->window);
	Hooks::on_configure_request(*ev, c);

	if (c) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m;

	m = (Monitor *)ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	return m;
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawbar(Monitor *m)
{
	Hooks::on_draw_bar(*m);
	int x, w, tw = 0;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	if (!m->showbar)
		return;

	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		Clr statusscheme[3] = {
			*g_palette.xft(drw, core::ColorId::StatusFg),
			*g_palette.xft(drw, core::ColorId::StatusBg),
			*g_palette.xft(drw, core::ColorId::Border)
		};
		drw_setscheme(drw, statusscheme);
		const char *status_text = mods::status2d_plain(stext);
		tw = TEXTW(status_text) - lrpad + 2; /* 2px right padding */
		drw_text(drw, m->ww - tw, 0, tw, bh, 0, status_text, 0);
	}

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		const bool active_tag = (m->tagset[m->seltags] & (1 << i)) != 0;
		Clr tagscheme[3] = {
			*(active_tag ? g_palette.xft(drw, core::ColorId::TagActive) : g_palette.xft(drw, core::ColorId::TagInactive)),
			*g_palette.xft(drw, core::ColorId::Bg),
			*g_palette.xft(drw, core::ColorId::Border)
		};
		drw_setscheme(drw, tagscheme);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect(drw, x + boxs, boxs, boxw, boxw,
				m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
				urg & 1 << i);
		x += w;
	}
	w = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

	if ((w = m->ww - tw - x) > bh) {
		if (m->sel) {
			drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
			char titlebuf[512];
			snprintf(titlebuf, sizeof(titlebuf), "%s", m->sel->name);
			Hooks::format_client_title(*m->sel, titlebuf, sizeof(titlebuf));
			drw_text(drw, x, 0, w, bh, lrpad / 2, titlebuf, 0);
			if (m->sel->isfloating)
				drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		} else {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void
drawbars(void)
{
	Monitor *m;
	const int total_rows = static_cast<int>(LENGTH(keys));

	for (m = mons; m; m = m->next)
		drawbar(m);
	for (m = mons; m; m = m->next) {
		if (!m->hudwin)
			continue;
		if (!showhud) {
			XUnmapWindow(dpy, m->hudwin);
			continue;
		}
		const int hud_y = m->my + ((m->showbar && m->topbar) ? bh : 0);
		const int hud_h = m->mh - ((m->showbar && m->topbar) ? bh : 0);
		if (hud_h <= 0) {
			XUnmapWindow(dpy, m->hudwin);
			continue;
		}
		XMoveResizeWindow(dpy, m->hudwin, m->mx, hud_y, m->mw, hud_h);
		XMapRaised(dpy, m->hudwin);
		Clr hudscheme[3] = {
			scheme[SchemeNorm][ColFg],
			scheme[SchemeNorm][ColBg],
			scheme[SchemeNorm][ColBorder]
		};
		hudscheme[ColFg] = scheme[SchemeSel][ColFg];
		drw_setscheme(drw, scheme[SchemeNorm]);
		int lineh = drw->fonts->h + 2;
		int y = 0;
		const int header_rows = 2;
		const int content_y = lineh * header_rows;
		int visible_rows = (hud_h - content_y) / lineh;
		if (visible_rows < 1)
			visible_rows = 1;
		if (hud_selected_row < 0)
			hud_selected_row = 0;
		if (hud_selected_row >= total_rows)
			hud_selected_row = total_rows - 1;
		if (hud_scroll_top > hud_selected_row)
			hud_scroll_top = hud_selected_row;
		if (hud_selected_row >= hud_scroll_top + visible_rows)
			hud_scroll_top = hud_selected_row - visible_rows + 1;
		const int max_scroll = total_rows > visible_rows ? (total_rows - visible_rows) : 0;
		if (hud_scroll_top < 0)
			hud_scroll_top = 0;
		if (hud_scroll_top > max_scroll)
			hud_scroll_top = max_scroll;
		char line[1024];
		drw_rect(drw, 0, 0, m->mw, hud_h, 1, 1);
		snprintf(line, sizeof line, "HUD: [c]=palette | [l]=log | [c]=keys");
		drw_setscheme(drw, hudscheme);
		drw_text(drw, 6, y, m->mw - 12, lineh, 0, line, 0);
		y += lineh;
		if (hud_page == HudPageKeys) {
			if (hud_edit_capture == HudCaptureMod)
				snprintf(line, sizeof line, "Capture modifier: press combo, Enter confirms");
			else if (hud_edit_capture == HudCaptureKey)
				snprintf(line, sizeof line, "Capture key: press target key");
			else if (hud_edit_armed)
				snprintf(line, sizeof line, "Edit armed; press [m] modifier or [k] key");
			else
				snprintf(line, sizeof line, "%-18s %-12s %-24s %s", "Modifiers", "Key", "Command", "Description");
			drw_text(drw, 6, y, m->mw - 12, lineh, 0, line, 0);
			y += lineh;
			y = content_y;
			for (int row = hud_scroll_top; row < total_rows && row < hud_scroll_top + visible_rows; ++row) {
				const unsigned int i = static_cast<unsigned int>(row);
				char modbuf[128] = {0};
				unsigned int mod = keys[i].mod;
				if (mod & MODKEY) {
					strcat(modbuf, "MODKEY|");
					mod &= ~MODKEY;
				}
				if (mod & ShiftMask) strcat(modbuf, "Shift|");
				if (mod & ControlMask) strcat(modbuf, "Control|");
				if (mod & Mod1Mask) strcat(modbuf, "Mod1|");
				if (mod & Mod2Mask) strcat(modbuf, "Mod2|");
				if (mod & Mod3Mask) strcat(modbuf, "Mod3|");
				if (mod & Mod4Mask) strcat(modbuf, "Mod4|");
				if (mod & Mod5Mask) strcat(modbuf, "Mod5|");
				size_t len = strlen(modbuf);
				if (len > 0) modbuf[len - 1] = '\0'; else strcpy(modbuf, "None");
				const char *ks = XKeysymToString(keys[i].keysym);
				snprintf(line, sizeof line, "%-18s %-12s %-24s %s", modbuf, ks ? ks : "?", keys[i].cmd, keys[i].desc);
				drw_setscheme(drw, row == hud_selected_row ? scheme[SchemeSel] : scheme[SchemeNorm]);
				drw_text(drw, 6, y, m->mw - 12, lineh, 0, line, 0);
				y += lineh;
			}
		} else if (hud_page == HudPagePalette) {
			const char *chname = palette_channel == PaletteChannelR ? "R" : palette_channel == PaletteChannelG ? "G" : palette_channel == PaletteChannelB ? "B" : "A";
			snprintf(line, sizeof line, "Palette: Up/Down color, R/G/B/A channel, Left/Right edit, Enter apply (%s)", chname);
			drw_text(drw, 6, y, m->mw - 12, lineh, 0, line, 0);
			y += lineh;
			if (hud_warn[0] != '\0') {
				drw_setscheme(drw, scheme[SchemeSel]);
				drw_text(drw, 6, y, m->mw - 12, lineh, 0, hud_warn, 0);
				drw_setscheme(drw, hudscheme);
				y += lineh;
			}
			int prow = 0;
			int pvisible = (hud_h - y) / lineh;
			if (pvisible < 1) pvisible = 1;
			int pscroll = palette_selected_row - pvisible / 2;
			if (pscroll < 0) pscroll = 0;
			const int pmaxscroll = (int)core::color_count > pvisible ? (int)core::color_count - pvisible : 0;
			if (pscroll > pmaxscroll) pscroll = pmaxscroll;
			for (int idx = pscroll; idx < (int)core::color_count && prow < pvisible; ++idx, ++prow) {
				const core::Color c = palette_draft[idx];
				snprintf(line, sizeof line, "%-12s  r=%.2f g=%.2f b=%.2f a=%.2f", color_name((core::ColorId)idx), c.r, c.g, c.b, c.a);
				drw_setscheme(drw, idx == palette_selected_row ? scheme[SchemeSel] : scheme[SchemeNorm]);
				drw_text(drw, 6, y, m->mw - 12, lineh, 0, line, 0);
				y += lineh;
			}
		} else {
			snprintf(line, sizeof line, "Log page. File path: %s", core::hud_log_path());
			drw_text(drw, 6, y, m->mw - 12, lineh, 0, line, 0);
			y += lineh;
			const std::size_t dump_size = g_hud_log.size();
			int total_lines = 0;
			for (std::size_t i = 0; i < dump_size; ++i)
				if (g_hud_log.at(i) == '\n')
					++total_lines;
			if (dump_size > 0 && g_hud_log.at(dump_size - 1) != '\n')
				++total_lines;
			int visible = (hud_h - y) / lineh;
			if (visible < 1) visible = 1;
			if (hud_log_scroll < 0) hud_log_scroll = 0;
			int max_scroll = total_lines > visible ? (total_lines - visible) : 0;
			if (hud_log_scroll > max_scroll) hud_log_scroll = max_scroll;
			int line_idx = 0;
			int shown = 0;
			std::size_t start = 0;
			for (std::size_t i = 0; i <= dump_size; ++i) {
				if (i == dump_size || g_hud_log.at(i) == '\n') {
					if (line_idx >= hud_log_scroll && shown < visible) {
						std::size_t len = i - start;
						if (len >= sizeof(line)) len = sizeof(line) - 1;
						for (std::size_t j = 0; j < len; ++j)
							line[j] = g_hud_log.at(start + j);
						line[len] = '\0';
						drw_setscheme(drw, scheme[SchemeNorm]);
						drw_text(drw, 6, y, m->mw - 12, lineh, 0, line, 0);
						y += lineh;
						++shown;
					}
					++line_idx;
					start = i + 1;
				}
				if (shown >= visible)
					break;
			}
		}
		drw_map(drw, m->hudwin, 0, 0, m->mw, hud_h);
	}
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbars();
}

void
focus(Client *c) noexcept
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	if (c)
		Hooks::on_focus(*c);
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
		return;
	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
	int format;
	unsigned long nitems, dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &format, &nitems, &dl, &p) == Success && p) {
		if (nitems > 0 && format == 32)
			atom = *(long *)p;
		XFree(p);
	}
	return atom;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, &p) != Success)
		return -1;
	if (n != 0 && format == 32)
		result = *(long *)p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING) {
		strncpy(text, (char *)name.value, size - 1);
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
		strncpy(text, *list, size - 1);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j, k;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		int start, end, skip;
		KeySym *syms;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		XDisplayKeycodes(dpy, &start, &end);
		syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
		if (!syms)
			return;
		for (k = start; k <= end; k++)
			for (i = 0; i < LENGTH(keys); i++)
				/* skip modifier codes, we do that ourselves */
				if (keys[i].keysym == syms[(k - start) * skip])
					for (j = 0; j < LENGTH(modifiers); j++)
						XGrabKey(dpy, k,
							 keys[i].mod | modifiers[j],
							 root, True,
							 GrabModeAsync, GrabModeAsync);
		XFree(syms);
	}
	update_hud_keygrabs();
}

void
update_hud_keygrabs(void)
{
	unsigned int j;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	const KeyCode up = XKeysymToKeycode(dpy, XK_Up);
	const KeyCode down = XKeysymToKeycode(dpy, XK_Down);
	for (j = 0; j < LENGTH(modifiers); ++j) {
		XUngrabKey(dpy, up, modifiers[j], root);
		XUngrabKey(dpy, down, modifiers[j], root);
	}
	if (!showhud)
		return;
	for (j = 0; j < LENGTH(modifiers); ++j) {
		XGrabKey(dpy, up, modifiers[j], root, True, GrabModeAsync, GrabModeAsync);
		XGrabKey(dpy, down, modifiers[j], root, True, GrabModeAsync, GrabModeAsync);
	}
}

void
grab_hud_keyboard(void)
{
	if (hud_keyboard_grabbed)
		return;
	if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
		hud_keyboard_grabbed = 1;
}

void
ungrab_hud_keyboard(void)
{
	if (!hud_keyboard_grabbed)
		return;
	XUngrabKeyboard(dpy, CurrentTime);
	hud_keyboard_grabbed = 0;
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	Hooks::on_key(*ev);
	keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, 0);
	if (showhud) {
		if (keysym == XK_l && hud_edit_capture == HudCaptureNone) {
			hud_page = HudPageLog;
			drawbars();
			return;
		}
		if (keysym == XK_c && hud_edit_capture == HudCaptureNone) {
			hud_page = (hud_page == HudPageKeys) ? HudPagePalette : HudPageKeys;
			hud_warn[0] = '\0';
			drawbars();
			return;
		}
		if (hud_page == HudPageLog) {
			if (keysym == XK_Up) {
				if (hud_log_scroll > 0) --hud_log_scroll;
				drawbars();
				return;
			}
			if (keysym == XK_Down) {
				++hud_log_scroll;
				drawbars();
				return;
			}
			return;
		}
		if (hud_page == HudPagePalette) {
			const float step = (CLEANMASK(ev->state) & ShiftMask) ? 0.05f : 0.01f;
			if (keysym == XK_Up) {
				if (palette_selected_row > 0) --palette_selected_row;
				drawbars();
				return;
			}
			if (keysym == XK_Down) {
				if (palette_selected_row + 1 < (int)core::color_count) ++palette_selected_row;
				drawbars();
				return;
			}
			if (keysym == XK_r) { palette_channel = PaletteChannelR; drawbars(); return; }
			if (keysym == XK_g) { palette_channel = PaletteChannelG; drawbars(); return; }
			if (keysym == XK_b) { palette_channel = PaletteChannelB; drawbars(); return; }
			if (keysym == XK_a) { palette_channel = PaletteChannelA; drawbars(); return; }
			if (keysym == XK_Left || keysym == XK_Right) {
				core::Color &c = palette_draft[palette_selected_row];
				const float delta = (keysym == XK_Right) ? step : -step;
				if (palette_channel == PaletteChannelR) c.r += delta;
				else if (palette_channel == PaletteChannelG) c.g += delta;
				else if (palette_channel == PaletteChannelB) c.b += delta;
				else c.a += delta;
				c = core::color_clamp(c);
				palette_dirty = 1;
				drawbars();
				return;
			}
			if (keysym == XK_Return) {
				const core::ColorId id = static_cast<core::ColorId>(palette_selected_row);
				if (palette_try_set_with_guard(id, palette_draft[palette_selected_row], hud_warn, sizeof(hud_warn))) {
					save_runtime_palette();
					palette_draft[palette_selected_row] = g_palette.get(id);
					palette_dirty = 0;
					arrange(nullptr);
				}
				drawbars();
				return;
			}
			return;
		}
		if (hud_edit_capture == HudCaptureMod) {
			if (keysym == XK_Return) {
				save_runtime_key_bindings();
				grabkeys();
				hud_edit_capture = HudCaptureNone;
				hud_edit_armed = 0;
				drawbars();
				return;
			}
			if (hud_selected_row >= 0 && hud_selected_row < static_cast<int>(LENGTH(keys))) {
				unsigned int cleaned = CLEANMASK(ev->state);
				if (keysym == XK_Shift_L || keysym == XK_Shift_R)
					cleaned |= ShiftMask;
				else if (keysym == XK_Control_L || keysym == XK_Control_R)
					cleaned |= ControlMask;
				else if (keysym == XK_Alt_L || keysym == XK_Alt_R || keysym == XK_Meta_L || keysym == XK_Meta_R)
					cleaned |= Mod1Mask;
				else if (keysym == XK_Super_L || keysym == XK_Super_R)
					cleaned |= Mod4Mask;
				keys[hud_selected_row].mod = cleaned;
			}
			drawbars();
			return;
		}
		if (hud_edit_capture == HudCaptureKey) {
			if (hud_selected_row >= 0 && hud_selected_row < static_cast<int>(LENGTH(keys))) {
				keys[hud_selected_row].keysym = keysym;
				save_runtime_key_bindings();
				grabkeys();
			}
			hud_edit_capture = HudCaptureNone;
			hud_edit_armed = 0;
			drawbars();
			return;
		}
		for (i = 0; i < LENGTH(keys); ++i) {
			if (!keys[i].cmd)
				continue;
			if (strcmp(keys[i].cmd, "core:toggle_hud") == 0
				&& keysym == keys[i].keysym
				&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)) {
				keys[i].func(&(keys[i].arg));
				return;
			}
		}
		if (keysym == XK_Return) {
			if (hud_edit_capture == HudCaptureNone)
				hud_edit_armed = !hud_edit_armed;
			drawbars();
			return;
		}
		if (hud_edit_armed && hud_edit_capture == HudCaptureNone) {
			if (keysym == XK_m) {
				hud_edit_capture = HudCaptureMod;
				if (hud_selected_row >= 0 && hud_selected_row < static_cast<int>(LENGTH(keys)))
					keys[hud_selected_row].mod = 0;
				drawbars();
				return;
			}
			if (keysym == XK_k) {
				hud_edit_capture = HudCaptureKey;
				drawbars();
				return;
			}
		}
		if (keysym == XK_Up) {
			if (hud_selected_row > 0)
				--hud_selected_row;
			drawbars();
			return;
		}
		if (keysym == XK_Down) {
			if (hud_selected_row + 1 < static_cast<int>(LENGTH(keys)))
				++hud_selected_row;
			drawbars();
			return;
		}
		return;
	}
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
manage(Window w, XWindowAttributes *wa) noexcept
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = (Client *)ecalloc(1, sizeof(Client));
	Hooks::on_manage(*c);
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		Hooks::before_manage_rules(*c);
		applyrules(c);
	}

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	focus(NULL);
	Hooks::after_manage(*c);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;
	c = wintoclient(ev->window);
	Hooks::on_property_notify(*ev, c);

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if (c) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			c->hintsvalid = 0;
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg)
{
	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resize(Client *c, int x, int y, int w, int h, int interact) noexcept
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		apply_client_geometry(c, x, y, w, h, interact);
}

void
apply_client_geometry(Client *c, int x, int y, int w, int h, int interact)
{
	core::Geometry g = { { x, y }, { w, h } };
	(void)interact;
	Hooks::before_apply_geometry(*c, g);
	resizeclient(c, x, y, w, h);
	Hooks::after_apply_geometry(*c);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;
	core::Geometry g = { { x, y }, { w, h } };
	Hooks::before_resize_client(*c, g);

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
	Hooks::after_resize_client(*c);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void) noexcept
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev)) {
		wm_transient_reset();
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
	}
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	if (c->isfullscreen)
		resizeclient(c, m->mx, m->my, m->mw, m->mh);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
	if (!c->neverfocus)
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&c->win, 1);
	sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (mods::is_fakefullscreen(c)) {
		c->isfullscreen = fullscreen;
		if (fullscreen) {
			XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		} else {
			XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char*)0, 0);
		}
		return;
	}
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	Hooks::on_set_layout(*selmon);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = f;
	Hooks::on_set_mfact(*selmon);
	arrange(selmon);
}

void
setup(void) noexcept
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;
	struct sigaction sa;
	init_handlers();
	Hooks::on_setup();

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies (inherited from .xinitrc etc) immediately */
	while (waitpid(-1, NULL, WNOHANG) > 0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 2;
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = (Clr **)ecalloc(2, sizeof(Clr *));
	for (i = 0; i < 2; i++)
		scheme[i] = (Clr *)ecalloc(3, sizeof(Clr));
	apply_palette_to_scheme();
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
spawn(const Arg *arg)
{
	struct sigaction sa;

	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
	}
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
tile(Monitor *m) noexcept
{
	unsigned int i, n, h, mw, my, ty;
	Client *c;
	int gap = mods::gaps_px();
	int wx = m->wx + gap;
	int wy = m->wy + gap;
	int ww = m->ww - 2 * gap;
	int wh = m->wh - 2 * gap;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;
	if (ww <= 0 || wh <= 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? ww * m->mfact : 0;
	else
		mw = ww;
	for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			h = (wh - my) / (MIN(n, m->nmaster) - i);
			resize(c, wx, wy + my, MAX(1, mw - (2 * c->bw) - gap), MAX(1, h - (2 * c->bw) - gap), 0);
			if (my + HEIGHT(c) + gap < (unsigned int)wh)
				my += HEIGHT(c) + gap;
		} else {
			h = (wh - ty) / (n - i);
			resize(c, wx + mw + gap, wy + ty, MAX(1, ww - mw - (2 * c->bw) - gap), MAX(1, h - (2 * c->bw) - gap), 0);
			if (ty + HEIGHT(c) + gap < (unsigned int)wh)
				ty += HEIGHT(c) + gap;
		}
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
	arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
			selmon->sel->w, selmon->sel->h, 0);
	arrange(selmon);
}

void
togglehud(const Arg *arg)
{
	(void)arg;
	showhud = !showhud;
	if (!showhud) {
		hud_edit_armed = 0;
		hud_edit_capture = HudCaptureNone;
		ungrab_hud_keyboard();
	} else {
		hud_page = HudPageKeys;
		for (std::size_t i = 0; i < core::color_count; ++i)
			palette_draft[i] = g_palette.get(static_cast<core::ColorId>(i));
		hud_warn[0] = '\0';
		palette_dirty = 0;
		grab_hud_keyboard();
	}
	update_hud_keygrabs();
	drawbars();
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Hooks::on_unmanage(*c);
	Monitor *m = c->mon;
	XWindowChanges wc;

	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	/* arena allocator keeps memory until process exit */
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
}

void
updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa;
	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ButtonPressMask|ExposureMask;
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
				CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
		m->hudwin = XCreateWindow(dpy, root, m->wx, m->wy, m->ww, m->wh, 0, DefaultDepth(dpy, screen),
				CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->hudwin, cursor[CurNormal]->cursor);
		XUnmapWindow(dpy, m->hudwin);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	} else
		m->by = -bh;
}

void
updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->win), 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = (XineramaScreenInfo *)ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;

		/* new monitors if nn > n */
		for (i = n; i < nn; i++) {
			for (m = mons; m && m->next; m = m->next);
			if (m)
				m->next = createmon();
			else
				mons = createmon();
		}
		for (i = 0, m = mons; i < nn && m; m = m->next, i++)
			if (i >= n
			|| unique[i].x_org != m->mx || unique[i].y_org != m->my
			|| unique[i].width != m->mw || unique[i].height != m->mh)
			{
				dirty = 1;
				m->num = i;
				m->mx = m->wx = unique[i].x_org;
				m->my = m->wy = unique[i].y_org;
				m->mw = m->ww = unique[i].width;
				m->mh = m->wh = unique[i].height;
				updatebarpos(m);
			}
		/* removed monitors if n > nn */
		for (i = nn; i < n; i++) {
			for (m = mons; m && m->next; m = m->next);
			while ((c = m->clients)) {
				dirty = 1;
				m->clients = c->next;
				detachstack(c);
				c->mon = mons;
				attach(c);
				attachstack(c);
			}
			if (m == selmon)
				selmon = mons;
			cleanupmon(m);
		}
		/* arena allocator keeps memory until process exit */
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-" VERSION);
	Hooks::on_update_status(stext);
	drawbars();
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	Hooks::on_view(*selmon);
	focus(NULL);
	arrange(selmon);
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin || w == m->hudwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
		return;
	if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-" VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	checkotherwm();
	load_hud_log();
	core::log_line("startup: display opened");
	init_runtime_key_bindings();
	load_runtime_key_bindings();
	load_runtime_palette();
	Hooks::on_start();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
