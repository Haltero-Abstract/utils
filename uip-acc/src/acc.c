/*
 * uIP tool - desk accessory front end (UIP.ACC)
 *
 * Runs the uIPtool network stack (HTTP + FTP server over NetUSBee)
 * in the background of the GEM desktop.  Open it from the "Desk"
 * menu: a small window shows the IP address and lets you switch the
 * network ON / OFF and toggle DHCP / static IP.
 *
 * The network is serviced on AES timer events, so it runs while the
 * desktop (or any GEM program) is waiting for events.  When a program
 * takes over the machine (a game such as RICK3D, a TOS program), the
 * accessory simply sleeps until GEM is back.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <osbind.h>

#include "accgem.h"
#include "uipacc.h"

#define WIN_KIND   (NAME|CLOSER|MOVER)
#define COLS 33
#define ROWS 7

#define POLL_MS_ACTIVE 20     /* timer when idle network     */
#define POLL_MS_BUSY   0      /* timer when packets flow     */
#define HEAP_POOL      (160L*1024L)

#define AUTOSTART_FILE "UIPACC.INF"

long _stksize = 16384;           /* libcmini: accessory stack size */
extern void __mallocChunkSize(size_t siz);

char uipacc_lastlog[64];

/* ---- English / French, chosen from the TOS language ------------------ */
/* French strings use the Atari character set: \x82 = e acute            */
enum {
  S_TITLE, S_NET, S_ON, S_OFF, S_IP_NONE, S_IP_WAIT, S_MODE, S_STATIC,
  S_HINT, S_ERR_DRV, S_ERR_MINT, S_ERR_STIK, S_BTN_ON, S_BTN_OFF, S_BTN_MODE,
  S_NOWIN, S_LIC_TITLE, S_COUNT
};
static const char *const str_en[S_COUNT] = {
  "  uIP tool (network) ", "Network: %s", "ON", "OFF", "IP     : -",
  "IP     : waiting DHCP...", "Mode   : %s", "static IP (uip.cfg)",
  "Web http://IP  FTP port 21", "Error: NetUSBee not found?",
  "Error: not under MiNT", "Error: STiK/STinG loaded",
  "  Activate  ", " Deactivate ", "[DHCP/Static]",
  "[3][uIP tool:|no window available][ OK ]",
  " uIP tool - Help / About / Licences "
};
static const char *const str_fr[S_COUNT] = {
  "  uIP tool (r\x82seau) ", "R\x82seau : %s", "ACTIF", "INACTIF", "IP     : -",
  "IP     : attente DHCP...", "Mode   : %s", "IP fixe (uip.cfg)",
  "Web http://IP  FTP port 21", "Erreur: NetUSBee absente ?",
  "Erreur: pas sous MiNT", "Erreur: STiK/STinG charg\x82",
  "  Activer   ", " D\x82sactiver ", "[DHCP/Fixe]",
  "[3][uIP tool:|plus de fen\x88tre disponible][ OK ]",
  " uIP tool - Aide / \266 propos / Licences "
};
static const char *const *tr = str_en;
#define T(id) (tr[id])

/* ---- About / licences window: the full licence text is embedded ------ */
#include "license_text.h"
#include "help_en.h"
#include "help_fr.h"

static const char *const about_en[] = {
  "",
  "========================= NOTICES =========================",
  "",
  "uIP tool ACC - NetUSBee network in the background of GEM",
  "UNOFFICIAL version: not endorsed or maintained by the uIPtool authors.",
  "Official uIPtool: https://codeberg.org/nokturnal/uiptool",
  "",
  "Accessory front end: Haltero / Abstract 2026 - MIT licence.",
  "Written with the help of an AI assistant (Claude).",
  "libcmini (linked in): FreeMiNT project - LGPL.",
  "",
  "This product includes software developed by Adam Dunkels.",
  "This product includes software developed by egnite Software GmbH",
  "and its contributors.",
  "This product includes software developed by the University of",
  "California, Berkeley and its contributors.",
  "",
  "------------------------- uIPtool LICENSE --------------------------",
  "",
};
static const char *const about_fr[] = {
  "",
  "========================= MENTIONS =========================",
  "",
  "uIP tool ACC - r\202seau NetUSBee en arri\212re-plan du GEM",
  "Version NON OFFICIELLE : ni approuv\202e ni maintenue par les auteurs",
  "d'uIPtool. uIPtool officiel : https://codeberg.org/nokturnal/uiptool",
  "",
  "Accessoire : Haltero / Abstract 2026 - licence MIT.",
  "\220crit avec l'aide d'une IA (Claude).",
  "libcmini (incluse) : projet FreeMiNT - LGPL.",
  "",
  "Mentions obligatoires (texte original des licences) :",
  "This product includes software developed by Adam Dunkels.",
  "This product includes software developed by egnite Software GmbH",
  "and its contributors.",
  "This product includes software developed by the University of",
  "California, Berkeley and its contributors.",
  "",
  "------------------------- LICENCE uIPtool --------------------------",
  "",
};
#define ABOUT_EN_LINES ((int)(sizeof(about_en) / sizeof(about_en[0])))
#define ABOUT_FR_LINES ((int)(sizeof(about_fr) / sizeof(about_fr[0])))
static const char *const *about = about_en;
static int about_lines = ABOUT_EN_LINES;
static const char *const *help = help_en;
static int help_lines = HELP_EN_LINES;
/* window content: user guide, then notices, then the full licence */
#define LIC_TOTAL (help_lines + about_lines + LICENSE_TEXT_LINES)
static const char *lic_line(int i)
{
  if (i < help_lines) return help[i];
  i -= help_lines;
  if (i < about_lines) return about[i];
  return license_text[i - about_lines];
}

/* supervisor: 1 if the TOS language is French (France / Swiss French) */
static long detect_french(void)
{
  long *jar = *(long **)0x5a0;          /* _p_cookies */
  int lang = -1;
  if (jar) {
    for (; jar[0]; jar += 2)
      if (jar[0] == 0x5f414b50L) {      /* '_AKP': bits 8-15 = language */
        lang = (int)((jar[1] >> 8) & 0xff);
        break;
      }
  }
  if (lang < 0) {                       /* TOS header os_conf >> 1 = country */
    unsigned char *sysbase = *(unsigned char **)0x4f2;
    unsigned char *os = *(unsigned char **)(sysbase + 8);  /* os_beg */
    lang = (*(unsigned short *)(os + 0x1c)) >> 1;
  }
  return (lang == 2 || lang == 7);      /* 2 = France, 7 = Swiss French */
}

/* acccrit.s: set while TOS is inside a critical-error alert (GEMDOS busy) */
extern volatile short uipacc_in_critic;
extern long uipacc_install_critic(void);

static int ensure_vdi(void);
static void release_vdi(void);

static short ap_id, menu_id = -1;
static short vh = -1, phys_h;
static short char_w = 8, char_h = 16;
static short win = -1;
static short wx = -1, wy = -1;            /* remembered window position */
static struct uipacc_status st, shown;
static int last_err = 0;
static int autostart = 0;
static char inf_path[64];
static char cfg_path[64];
static long dbg_ticks; static short dbg_ev, dbg_btn, dbg_top;

/* ------------------------------------------------------------------ */
/* printf() of the stack lands here (see accshim.h)                   */
int uipacc_log(const char *fmt, ...)
{
  char tmp[128];
  char *s, *d;
  va_list ap;
  int n;
  va_start(ap, fmt);
  n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  /* strip VT52 escapes and CR/LF, keep the text */
  d = uipacc_lastlog;
  for (s = tmp; *s && d < uipacc_lastlog + sizeof(uipacc_lastlog) - 1; s++) {
    if (*s == 27) { if (s[1]) s++; continue; }
    if (*s == '\r' || *s == '\n') continue;
    *d++ = *s;
  }
  if (d != uipacc_lastlog) *d = 0;
  return n;
}

/* ------------------------------------------------------------------ */
/* Run a function in supervisor mode on OUR stack (like the original  */
/* uIPtool which runs entirely after Super(0)), keeping the GEMDOS    */
/* DTA of the foreground application intact.                          */
static long run_super(long (*fn)(void))
{
  long r;
  if (uipacc_in_critic) return 0;   /* GEMDOS busy: never re-enter it */
  void *old_dta = (void *)Fgetdta();
  long old_ssp = Super(0L);
  r = fn();
  Super((void *)old_ssp);
  Fsetdta(old_dta);
  return r;
}

/* ------------------------------------------------------------------ */
static void save_autostart(void)
{
  long fh = Fcreate(inf_path, 0);
  if (fh >= 0) {
    Fwrite((short)fh, 1, st.active ? "1" : "0");
    Fclose((short)fh);
  }
}

static void load_autostart(void)
{
  char c = '0';
  long fh = Fopen(inf_path, 0);
  if (fh >= 0) {
    Fread((short)fh, 1, &c);
    Fclose((short)fh);
  }
  autostart = (c == '1');
}

/* ------------------------------------------------------------------ */
static void fmt_line(char *out, int row)
{
  out[0] = 0;
  switch (row) {
  case 0:
    sprintf(out, T(S_NET), st.active ? T(S_ON) : T(S_OFF));
    break;
  case 1:
    if (!st.active)
      strcpy(out, T(S_IP_NONE));
    else if (st.ip[0] == 0 && !st.static_ip)
      strcpy(out, T(S_IP_WAIT));
    else
      sprintf(out, "IP     : %u.%u.%u.%u", st.ip[0], st.ip[1], st.ip[2], st.ip[3]);
    break;
  case 2:
    sprintf(out, T(S_MODE), st.static_ip ? T(S_STATIC) : "DHCP");
    break;
  case 3:
    if (st.active)
      strcpy(out, T(S_HINT));
    else if (last_err == UIPACC_ERR_DRIVER)
      strcpy(out, T(S_ERR_DRV));
    else if (last_err == UIPACC_ERR_MINT)
      strcpy(out, T(S_ERR_MINT));
    else if (last_err == UIPACC_ERR_STIK)
      strcpy(out, T(S_ERR_STIK));
    break;
  case 4:
    strncpy(out, uipacc_lastlog, COLS);
    out[COLS] = 0;
    break;
#ifdef UIPACC_FAKE_NIC
  case 5:
    sprintf(out, "dbg t=%ld e=%x b=%d top=%d/%d", dbg_ticks, dbg_ev, dbg_btn, dbg_top, win);
    break;
#endif
  case 6:
    sprintf(out, "[%s] %s [?]",
            st.active ? T(S_BTN_OFF) : T(S_BTN_ON), T(S_BTN_MODE));
    break;
  }
}

/* button hit areas, in character cells on row 6 */
#define BTN1_C0 0
#define BTN1_C1 14
#define BTN2_C0 15
#define BTN2_C1 ((int)(BTN2_C0 + strlen(T(S_BTN_MODE)) - 1))
#define BTN3_C0 (BTN2_C1 + 2)
#define BTN3_C1 (BTN3_C0 + 2)

static void draw_rect(short wkx, short wky, short wkw, short wkh, const short *clip)
{
  short r[4];
  char line[COLS + 8];
  int row;

  vs_clip(vh, 1, clip);
  vsf_interior(vh, 1);
  vsf_color(vh, 0);
  vsf_perimeter(vh, 0);
  r[0] = wkx; r[1] = wky; r[2] = wkx + wkw - 1; r[3] = wky + wkh - 1;
  vr_recfl(vh, r);

  vswr_mode(vh, 2);  /* transparent */
  vst_color(vh, 1);
  vst_alignment(vh, 0, 5); /* left, top */
  for (row = 0; row < ROWS; row++) {
    fmt_line(line, row);
    if (line[0])
      v_gtext(vh, wkx + char_w / 2, wky + 2 + row * char_h, line);
  }
  /* boxes around the two buttons */
  {
    short by = wky + 2 + 6 * char_h;
    v_box(vh, wkx + char_w / 2 + BTN1_C0 * char_w - 1, by - 1,
              wkx + char_w / 2 + BTN1_C1 * char_w, by + char_h);
    v_box(vh, wkx + char_w / 2 + BTN2_C0 * char_w - 1, by - 1,
              wkx + char_w / 2 + (BTN2_C1 + 1) * char_w, by + char_h);
    v_box(vh, wkx + char_w / 2 + BTN3_C0 * char_w - 1, by - 1,
              wkx + char_w / 2 + (BTN3_C1 + 1) * char_w, by + char_h);
  }
  vs_clip(vh, 0, clip);
}

static int intersect(const short *a, short *b) /* b = a ∩ b, x,y,w,h */
{
  short x1 = a[0] > b[0] ? a[0] : b[0];
  short y1 = a[1] > b[1] ? a[1] : b[1];
  short x2 = (a[0] + a[2] < b[0] + b[2]) ? a[0] + a[2] : b[0] + b[2];
  short y2 = (a[1] + a[3] < b[1] + b[3]) ? a[1] + a[3] : b[1] + b[3];
  b[0] = x1; b[1] = y1; b[2] = x2 - x1; b[3] = y2 - y1;
  return b[2] > 0 && b[3] > 0;
}

static void redraw(short x, short y, short w, short h)
{
  short wk[4], rr[4], area[4], clip[4];
  if (win < 0) return;
  area[0] = x; area[1] = y; area[2] = w; area[3] = h;
  wind_get(win, WF_WORKXYWH, &wk[0], &wk[1], &wk[2], &wk[3]);
  graf_mouse(256); /* M_OFF */
  wind_update(BEG_UPDATE);
  wind_get(win, WF_FIRSTXYWH, &rr[0], &rr[1], &rr[2], &rr[3]);
  while (rr[2] && rr[3]) {
    if (intersect(area, rr)) {
      clip[0] = rr[0]; clip[1] = rr[1];
      clip[2] = rr[0] + rr[2] - 1; clip[3] = rr[1] + rr[3] - 1;
      draw_rect(wk[0], wk[1], wk[2], wk[3], clip);
    }
    wind_get(win, WF_NEXTXYWH, &rr[0], &rr[1], &rr[2], &rr[3]);
  }
  wind_update(END_UPDATE);
  graf_mouse(257); /* M_ON */
}

static void redraw_all(void)
{
  short x, y, w, h;
  if (win < 0) return;
  wind_get(win, WF_WORKXYWH, &x, &y, &w, &h);
  redraw(x, y, w, h);
}

static void open_window(void)
{
  short wkw = COLS * char_w + char_w, wkh = ROWS * char_h + 6;
  short x, y, w, h, dx, dy, dw, dh;

  if (win >= 0) { wind_set(win, WF_TOP, 0, 0, 0, 0); return; }
  if (!ensure_vdi()) return;
  wind_calc(WC_BORDER, WIN_KIND, 0, 0, wkw, wkh, &dx, &dy, &w, &h);
  wind_get(0, WF_WORKXYWH, &dx, &dy, &dw, &dh); /* desktop area */
  if (wx < 0) { x = dx + dw - w - 8; y = dy + 8; }
  else { x = wx; y = wy; }
  win = wind_create(WIN_KIND, x, y, w, h);
  if (win < 0) {
    form_alert(1, T(S_NOWIN));
    win = -1;
    return;
  }
  wind_set_str(win, WF_NAME, " uIP tool ");
  wind_open(win, x, y, w, h);
}

static void close_window(int delete_only)
{
  if (win < 0) return;
  if (!delete_only) {
    short x, y, w, h;
    wind_get(win, WF_CURRXYWH, &x, &y, &w, &h);
    wx = x; wy = y;
    wind_close(win);
    wind_delete(win);
  }
  /* on AC_CLOSE the AES has already removed the window itself */
  win = -1;
  release_vdi();
}

static void refresh_status(int force)
{
  uipacc_get_status(&st);
  if (force || memcmp(&st, &shown, sizeof(st)) != 0) {
    shown = st;
    redraw_all();
  }
}

static void net_on(void)
{
  long r = run_super(uipacc_check_env);
  if (r == 0) r = run_super(uipacc_start);
  last_err = (int)r;
  refresh_status(1);
}

static void net_off(void)
{
  run_super(uipacc_stop);
  last_err = 0;
  refresh_status(1);
}

static void wait_release(void)
{
  short b[8], x, y, mb, k, kr, nc;
  /* MU_BUTTON with state 0 = wait until the left button is released */
  evnt_multi(MU_BUTTON, 1, 1, 0, 0, b, &x, &y, &mb, &k, &kr, &nc);
}

/* ------------------------------------------------------------------ */
/* About / licences window: scrollable view of the embedded texts     */
#define LIC_KIND (NAME|CLOSER|FULLER|MOVER|SIZER|UPARROW|DNARROW|VSLIDE)
static short lic_win = -1;
static int lic_top = 0;
static short lic_full = 0, lic_px, lic_py, lic_pw, lic_ph;

static int lic_visible(void)
{
  short x, y, w, h;
  wind_get(lic_win, WF_WORKXYWH, &x, &y, &w, &h);
  return h / char_h > 0 ? h / char_h : 1;
}

static void lic_clamp(void)
{
  int maxtop = LIC_TOTAL - lic_visible();
  if (maxtop < 0) maxtop = 0;
  if (lic_top > maxtop) lic_top = maxtop;
  if (lic_top < 0) lic_top = 0;
}

static void lic_slider(void)
{
  int vis = lic_visible(), maxtop = LIC_TOTAL - vis;
  long size = (1000L * vis) / LIC_TOTAL;
  if (size > 1000) size = 1000;
  wind_set(lic_win, WF_VSLSIZE, (short)size, 0, 0, 0);
  wind_set(lic_win, WF_VSLIDE, maxtop > 0 ? (short)((1000L * lic_top) / maxtop) : 0, 0, 0, 0);
}

static void lic_draw(short wkx, short wky, short wkw, short wkh, const short *clip)
{
  short r[4];
  int row, rows = wkh / char_h + 1;
  vs_clip(vh, 1, clip);
  vsf_interior(vh, 1); vsf_color(vh, 0); vsf_perimeter(vh, 0);
  r[0] = wkx; r[1] = wky; r[2] = wkx + wkw - 1; r[3] = wky + wkh - 1;
  vr_recfl(vh, r);
  vswr_mode(vh, 2); vst_color(vh, 1); vst_alignment(vh, 0, 5);
  for (row = 0; row < rows && lic_top + row < LIC_TOTAL; row++) {
    const char *l = lic_line(lic_top + row);
    if (l[0]) v_gtext(vh, wkx + char_w / 2, wky + row * char_h, l);
  }
  vs_clip(vh, 0, clip);
}

static void lic_redraw(short x, short y, short w, short h)
{
  short wk[4], rr[4], area[4], clip[4];
  if (lic_win < 0) return;
  area[0] = x; area[1] = y; area[2] = w; area[3] = h;
  wind_get(lic_win, WF_WORKXYWH, &wk[0], &wk[1], &wk[2], &wk[3]);
  graf_mouse(256);
  wind_update(BEG_UPDATE);
  wind_get(lic_win, WF_FIRSTXYWH, &rr[0], &rr[1], &rr[2], &rr[3]);
  while (rr[2] && rr[3]) {
    if (intersect(area, rr)) {
      clip[0] = rr[0]; clip[1] = rr[1];
      clip[2] = rr[0] + rr[2] - 1; clip[3] = rr[1] + rr[3] - 1;
      lic_draw(wk[0], wk[1], wk[2], wk[3], clip);
    }
    wind_get(lic_win, WF_NEXTXYWH, &rr[0], &rr[1], &rr[2], &rr[3]);
  }
  wind_update(END_UPDATE);
  graf_mouse(257);
}

static void lic_redraw_all(void)
{
  short x, y, w, h;
  wind_get(lic_win, WF_WORKXYWH, &x, &y, &w, &h);
  lic_redraw(x, y, w, h);
}

static void lic_scroll_to(int top)
{
  int old = lic_top;
  lic_top = top;
  lic_clamp();
  if (lic_top != old) { lic_slider(); lic_redraw_all(); }
}

static int ensure_vdi(void)
{
  if (vh < 0) {
    vh = v_opnvwk(phys_h);
    if (vh <= 0) { vh = -1; return 0; }
  }
  return 1;
}

static void release_vdi(void)
{
  if (win < 0 && lic_win < 0 && vh > 0) { v_clsvwk(vh); vh = -1; }
}

static void open_lic_window(void)
{
  short dx, dy, dw, dh, x, y, w, h, d;
  if (lic_win >= 0) { wind_set(lic_win, WF_TOP, 0, 0, 0, 0); return; }
  if (!ensure_vdi()) return;
  wind_get(0, WF_WORKXYWH, &dx, &dy, &dw, &dh);
  wind_calc(WC_BORDER, LIC_KIND, 0, 0, (LIC_COLS + 2) * char_w, 20 * char_h, &d, &d, &w, &h);
  if (w > dw) w = dw;
  if (h > dh) h = dh;
  x = dx + (dw - w) / 2; y = dy + (dh - h) / 2;
  lic_win = wind_create(LIC_KIND, dx, dy, dw, dh);
  if (lic_win < 0) { lic_win = -1; form_alert(1, T(S_NOWIN)); release_vdi(); return; }
  lic_top = 0; lic_full = 0;
  wind_set_str(lic_win, WF_NAME, T(S_LIC_TITLE));
  wind_open(lic_win, x, y, w, h);
  lic_slider();
}

static void close_lic_window(int delete_only)
{
  if (lic_win < 0) return;
  if (!delete_only) { wind_close(lic_win); wind_delete(lic_win); }
  lic_win = -1;
  release_vdi();
}

static void lic_message(short *buf)
{
  short x, y, w, h;
  switch (buf[0]) {
  case WM_REDRAW: lic_redraw(buf[4], buf[5], buf[6], buf[7]); break;
  case WM_TOPPED: wind_set(lic_win, WF_TOP, 0, 0, 0, 0); break;
  case WM_CLOSED: close_lic_window(0); break;
  case WM_MOVED:
    wind_set(lic_win, WF_CURRXYWH, buf[4], buf[5], buf[6], buf[7]);
    break;
  case WM_SIZED:
    if (buf[6] < 20 * char_w) buf[6] = 20 * char_w;
    if (buf[7] < 6 * char_h) buf[7] = 6 * char_h;
    wind_set(lic_win, WF_CURRXYWH, buf[4], buf[5], buf[6], buf[7]);
    lic_clamp(); lic_slider(); lic_redraw_all();
    break;
  case WM_FULLED:
    if (!lic_full) {
      wind_get(lic_win, WF_CURRXYWH, &lic_px, &lic_py, &lic_pw, &lic_ph);
      wind_get(0, WF_WORKXYWH, &x, &y, &w, &h);
      wind_set(lic_win, WF_CURRXYWH, x, y, w, h);
    } else {
      wind_set(lic_win, WF_CURRXYWH, lic_px, lic_py, lic_pw, lic_ph);
    }
    lic_full = !lic_full;
    lic_clamp(); lic_slider(); lic_redraw_all();
    break;
  case WM_ARROWED: {
    int vis = lic_visible();
    switch (buf[4]) {
    case 0: lic_scroll_to(lic_top - vis); break;   /* page up   */
    case 1: lic_scroll_to(lic_top + vis); break;   /* page down */
    case 2: lic_scroll_to(lic_top - 1); break;     /* line up   */
    case 3: lic_scroll_to(lic_top + 1); break;     /* line down */
    }
    break;
  }
  case WM_VSLID: {
    int maxtop = LIC_TOTAL - lic_visible();
    if (maxtop < 0) maxtop = 0;
    lic_scroll_to((int)(((long)buf[4] * maxtop + 500) / 1000));
    break;
  }
  }
}

static void handle_click(short mx, short my)
{
  short x, y, w, h;
  int col, row;
  if (win < 0 || uipacc_in_critic) return;
  wind_get(win, WF_WORKXYWH, &x, &y, &w, &h);
  if (mx < x || my < y || mx >= x + w || my >= y + h) return;
  col = (mx - x - char_w / 2) / char_w;
  row = (my - y - 2) / char_h;
  if (row != 6) return;
  wait_release();
  if (col >= BTN1_C0 && col <= BTN1_C1) {
    if (st.active) net_off(); else net_on();
    save_autostart();
  } else if (col >= BTN2_C0 && col <= BTN2_C1) {
    run_super(uipacc_toggle_ipmode);
    refresh_status(1);
  } else if (col >= BTN3_C0 && col <= BTN3_C1) {
    open_lic_window();
  }
}

/* ------------------------------------------------------------------ */
int main(void)
{
  short buf[8], mx, my, mb, ks, kr, nc, ev, dummy;
  int btn_was_down = 0;
  char drv;

  /* Private heap: memory Malloc()ed now belongs to the accessory for
   * the whole session; later Malloc()s would belong to whatever
   * program is in front and be freed when it quits. */
  {
    void *p = malloc(HEAP_POOL);
    if (p) free(p);
    __mallocChunkSize(16384);
  }

  Supexec(uipacc_install_critic);
  ap_id = appl_init();
  phys_h = graf_handle(&char_w, &char_h, &dummy, &dummy);
  if (char_h < 8) char_h = 16;

  /* config files on the boot drive */
  {
    long bootdev = 0;
    long old = Super(0L);
    bootdev = *(volatile short *)0x446; /* _bootdev */
    Super((void *)old);
    drv = 'A' + (char)bootdev;
  }
  sprintf(inf_path, "%c:\\%s", drv, AUTOSTART_FILE);
  sprintf(cfg_path, "%c:\\UIP.CFG", drv);
  uipacc_set_config_path(cfg_path);
  strcpy(uipacc_lastlog, "");

  if (Supexec(detect_french)) {
    tr = str_fr; about = about_fr; about_lines = ABOUT_FR_LINES;
    help = help_fr; help_lines = HELP_FR_LINES;
  }
  menu_id = menu_register(ap_id, T(S_TITLE));

  load_autostart();
  if (autostart) net_on(); else refresh_status(1);

  short top = -1;
  for (;;) {
    short flags = MU_MESAG;
    unsigned long ms = 0;
    short d1, d2, d3;
    top = -1;

    if (st.active) flags |= MU_TIMER;
    if (win >= 0) {
      wind_get(0, WF_TOP, &top, &d1, &d2, &d3);
      if (top == win) flags |= MU_BUTTON;
    }
    ms = POLL_MS_ACTIVE;

    ev = evnt_multi(flags, 1, 1, 1, ms, buf, &mx, &my, &mb, &ks, &kr, &nc);
    dbg_ticks++; dbg_ev = ev; dbg_top = top;
#ifdef UIPACC_FAKE_NIC
    if ((dbg_ticks & 15) == 0) redraw_all();
#endif

    if ((ev & MU_MESAG) && lic_win >= 0 && buf[0] >= WM_REDRAW && buf[0] <= WM_SIZED
        && buf[3] == lic_win) {
      lic_message(buf);
    } else if (ev & MU_MESAG) {
      switch (buf[0]) {
      case AC_OPEN:
        if (buf[4] == menu_id) open_window();
        break;
      case AC_CLOSE:
        /* desktop or a program is starting: AES already took our window */
        if (buf[3] == menu_id) { close_window(1); close_lic_window(1); }
        break;
      case WM_REDRAW:
        if (buf[3] == win) redraw(buf[4], buf[5], buf[6], buf[7]);
        break;
      case WM_TOPPED:
        if (buf[3] == win) wind_set(win, WF_TOP, 0, 0, 0, 0);
        break;
      case WM_CLOSED:
        if (buf[3] == win) close_window(0);
        break;
      case WM_MOVED:
        if (buf[3] == win) {
          wind_set(win, WF_CURRXYWH, buf[4], buf[5], buf[6], buf[7]);
          wx = buf[4]; wy = buf[5];
        }
        break;
      }
    }

    if (ev & MU_BUTTON) {
      if (wind_find(mx, my) == win) handle_click(mx, my);
      btn_was_down = 1;
    } else if ((ev & MU_TIMER) && win >= 0 && top == win) {
      /* While the network runs, button events can be missed between
       * two timer wake-ups: sample the mouse and act on the press edge. */
      short gx, gy, gb, gk;
      graf_mkstate(&gx, &gy, &gb, &gk);
      dbg_btn = gb;
      if ((gb & 1) && !btn_was_down && wind_find(gx, gy) == win)
        handle_click(gx, gy);
      btn_was_down = gb & 1;
    } else if (!(ev & MU_BUTTON)) {
      btn_was_down = 0;
    }

    if (st.active && !uipacc_in_critic) {
      long n = run_super(uipacc_step);
      (void)n;
      refresh_status(0);
    }
  }
  return 0;
}
