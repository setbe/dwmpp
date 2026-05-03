/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "monospace:size=10" };
static const char dmenufont[]       = "monospace:size=10";

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class_     instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */
static const int refreshrate = 120;  /* refresh rate (per second) for client move/resize */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY Mod1Mask
#define MOD_ENABLE_ALWAYSCENTER 1
#define MOD_ENABLE_ATTACH_MODES 1
#define MOD_ENABLE_FAKEFULLSCREEN 1
#define MOD_ENABLE_GAPS 1
#define MOD_ENABLE_MOVESTACK 1
#define MOD_ENABLE_PERTAG 1
#define MOD_ENABLE_SCRATCHPAD 1
#define MOD_ENABLE_STATUS2D 1
#define MOD_ENABLE_TITLESTATS 1
#define CTCMD(NAME) command_fn<NAME>()
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      CTCMD("core:view"),        {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      CTCMD("core:toggle_view"), {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      CTCMD("core:tag"),         {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      CTCMD("core:toggle_tag"),  {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, NULL };
static const char *termcmd[]  = { "st", NULL };

static constexpr Key default_keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      CTCMD("core:spawn"),           {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             XK_Return, CTCMD("core:spawn"),           {.v = termcmd } },
	{ MODKEY,                       XK_b,      CTCMD("core:toggle_bar"),      {0} },
	{ MODKEY,                       XK_j,      CTCMD("core:focus_next"),      {0} },
	{ MODKEY,                       XK_k,      CTCMD("core:focus_prev"),      {0} },
	{ MODKEY,                       XK_i,      CTCMD("pertag:inc_nmaster"),   {.i = +1 } },
	{ MODKEY,                       XK_d,      CTCMD("pertag:inc_nmaster"),   {.i = -1 } },
	{ MODKEY,                       XK_h,      CTCMD("pertag:set_mfact"),     {.f = -0.05} },
	{ MODKEY,                       XK_l,      CTCMD("pertag:set_mfact"),     {.f = +0.05} },
	{ MODKEY,                       XK_o,      CTCMD("core:toggle_hud"),      {0} },
	{ 0,                            0,         CTCMD("titlestats:toggle"),    {0} },
	{ MODKEY,                       XK_Return, CTCMD("core:zoom"),            {0} },
	{ MODKEY,                       XK_Tab,    CTCMD("core:view"),            {0} },
	{ MODKEY|ShiftMask,             XK_c,      CTCMD("core:kill_client"),     {0} },
	{ MODKEY|ControlMask,           XK_t,      CTCMD("pertag:set_layout"),    {.v = &layouts[0]} },
	{ MODKEY,                       XK_t,      CTCMD("core:set_layout"),      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      CTCMD("core:set_layout"),      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      CTCMD("core:set_layout"),      {.v = &layouts[2]} },
	{ MODKEY,                       XK_space,  CTCMD("core:set_layout"),      {0} },
	{ MODKEY|ShiftMask,             XK_space,  CTCMD("core:toggle_floating"), {0} },
	{ MODKEY,                       XK_0,      CTCMD("core:view"),            {.ui = ~0u } },
	{ MODKEY|ShiftMask,             XK_0,      CTCMD("core:tag"),             {.ui = ~0u } },
	{ MODKEY,                       XK_comma,  CTCMD("core:focusmon_prev"),   {.i = -1 } },
	{ MODKEY,                       XK_period, CTCMD("core:focusmon_next"),   {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  CTCMD("core:tagmon_prev"),     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, CTCMD("core:tagmon_next"),     {.i = +1 } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      CTCMD("core:quit"),            {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        CTCMD("core:set_layout"),      {0} },
	{ ClkLtSymbol,          0,              Button3,        CTCMD("core:set_layout"),      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        CTCMD("core:zoom"),            {0} },
	{ ClkStatusText,        0,              Button2,        CTCMD("core:spawn"),           {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        CTCMD("core:move_mouse"),      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        CTCMD("core:toggle_floating"), {0} },
	{ ClkClientWin,         MODKEY,         Button3,        CTCMD("core:resize_mouse"),    {0} },
	{ ClkTagBar,            0,              Button1,        CTCMD("core:view"),            {0} },
	{ ClkTagBar,            0,              Button3,        CTCMD("core:toggle_view"),     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        CTCMD("core:tag"),             {0} },
	{ ClkTagBar,            MODKEY,         Button3,        CTCMD("core:toggle_tag"),      {0} },
};
