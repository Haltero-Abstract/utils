#ifndef __ACCGEM_H__
#define __ACCGEM_H__

#define MU_KEYBD  0x01
#define MU_BUTTON 0x02
#define MU_MESAG  0x10
#define MU_TIMER  0x20

#define AC_OPEN   40
#define AC_CLOSE  41
#define WM_REDRAW 20
#define WM_TOPPED 21
#define WM_CLOSED 22
#define WM_MOVED  28
#define WM_FULLED   23
#define WM_ARROWED  24
#define WM_VSLID    26
#define WM_SIZED    27

#define NAME   0x0001
#define CLOSER 0x0002
#define MOVER  0x0008
#define FULLER  0x0004
#define SIZER   0x0020
#define UPARROW 0x0040
#define DNARROW 0x0080
#define VSLIDE  0x0100

#define WF_NAME       2
#define WF_WORKXYWH   4
#define WF_CURRXYWH   5
#define WF_FIRSTXYWH 11
#define WF_NEXTXYWH  12
#define WF_TOP       10
#define WF_VSLIDE     9
#define WF_VSLSIZE   16

#define WC_BORDER 0
#define WC_WORK   1
#define END_UPDATE 0
#define BEG_UPDATE 1

extern short aes_global[15];

short appl_init(void);
short menu_register(short id, const char *s);
short evnt_multi(short flags, short clicks, short mask, short state,
                 unsigned long ms, short *buf, short *mx, short *my,
                 short *mb, short *ks, short *kr, short *nc);
short form_alert(short def, const char *s);
short graf_handle(short *wc, short *hc, short *wb, short *hb);
short graf_mouse(short m);
short graf_mkstate(short *mx, short *my, short *mb, short *ks);
short wind_create(short kind, short x, short y, short w, short h);
short wind_open(short h, short x, short y, short w, short hh);
short wind_close(short h);
short wind_delete(short h);
short wind_get(short h, short f, short *a, short *b, short *c, short *d);
short wind_set(short h, short f, short a, short b, short c, short d);
short wind_set_str(short h, short f, const char *s);
short wind_find(short x, short y);
short wind_update(short m);
short wind_calc(short t, short k, short x, short y, short w, short h, short *ox, short *oy, short *ow, short *oh);

short v_opnvwk(short phys);
void v_clsvwk(short h);
void vs_clip(short h, short on, const short *r);
void vr_recfl(short h, const short *r);
void vsf_interior(short h, short s);
void vsf_color(short h, short c);
void vsf_perimeter(short h, short v);
void vswr_mode(short h, short m);
void vst_color(short h, short c);
void vst_alignment(short h, short hor, short ver);
void vst_effects(short h, short e);
void v_gtext(short h, short x, short y, const char *s);
void v_box(short h, short x1, short y1, short x2, short y2);

#endif
