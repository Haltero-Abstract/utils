/* Minimal AES / VDI bindings for the uIP accessory (no gemlib needed). */
#include "accgem.h"

short aes_control[5], aes_global[15], aes_intin[16], aes_intout[8];
void *aes_addrin[4], *aes_addrout[2];
static void *aespb[6] = { aes_control, aes_global, aes_intin, aes_intout, aes_addrin, aes_addrout };

short vdi_contrl[12], vdi_intin[128], vdi_ptsin[128], vdi_intout[128], vdi_ptsout[128];
static void *vdipb[5] = { vdi_contrl, vdi_intin, vdi_ptsin, vdi_intout, vdi_ptsout };

static short aes(short op, short nin, short nout, short ain, short aout)
{
  aes_control[0] = op; aes_control[1] = nin; aes_control[2] = nout;
  aes_control[3] = ain; aes_control[4] = aout;
  __asm__ volatile (
    "move.l %0,%%d1\n\t"
    "move.w #200,%%d0\n\t"
    "trap #2\n\t"
    : : "g"(aespb) : "d0","d1","d2","a0","a1","a2","memory","cc");
  return aes_intout[0];
}

static void vdi(short op, short nptsin, short nintin, short handle)
{
  vdi_contrl[0] = op; vdi_contrl[1] = nptsin; vdi_contrl[3] = nintin;
  vdi_contrl[6] = handle;
  __asm__ volatile (
    "move.l %0,%%d1\n\t"
    "move.w #115,%%d0\n\t"
    "trap #2\n\t"
    : : "g"(vdipb) : "d0","d1","d2","a0","a1","a2","memory","cc");
}

short appl_init(void) { return aes(10,0,1,0,0); }
short menu_register(short id, const char *s) { aes_intin[0]=id; aes_addrin[0]=(void*)s; return aes(35,1,1,1,0); }

short evnt_multi(short flags, short clicks, short mask, short state,
                 unsigned long ms, short *buf, short *mx, short *my,
                 short *mb, short *ks, short *kr, short *nc)
{
  short *ii = aes_intin;
  ii[0]=flags; ii[1]=clicks; ii[2]=mask; ii[3]=state;
  ii[4]=ii[5]=ii[6]=ii[7]=ii[8]=0; ii[9]=ii[10]=ii[11]=ii[12]=ii[13]=0;
  ii[14]=(short)(ms & 0xffff); ii[15]=(short)(ms >> 16);
  aes_addrin[0]=buf;
  aes(25,16,7,1,0);
  *mx=aes_intout[1]; *my=aes_intout[2]; *mb=aes_intout[3];
  *ks=aes_intout[4]; *kr=aes_intout[5]; *nc=aes_intout[6];
  return aes_intout[0];
}

short form_alert(short def, const char *s) { aes_intin[0]=def; aes_addrin[0]=(void*)s; return aes(52,1,1,1,0); }

short graf_handle(short *wc, short *hc, short *wb, short *hb)
{
  short r = aes(77,0,5,0,0);
  *wc=aes_intout[1]; *hc=aes_intout[2]; *wb=aes_intout[3]; *hb=aes_intout[4];
  return r;
}

short wind_create(short kind, short x, short y, short w, short h)
{ aes_intin[0]=kind; aes_intin[1]=x; aes_intin[2]=y; aes_intin[3]=w; aes_intin[4]=h; return aes(100,5,1,0,0); }
short wind_open(short h, short x, short y, short w, short hh)
{ aes_intin[0]=h; aes_intin[1]=x; aes_intin[2]=y; aes_intin[3]=w; aes_intin[4]=hh; return aes(101,5,1,0,0); }
short wind_close(short h) { aes_intin[0]=h; return aes(102,1,1,0,0); }
short wind_delete(short h) { aes_intin[0]=h; return aes(103,1,1,0,0); }
short wind_get(short h, short f, short *a, short *b, short *c, short *d)
{
  short r; aes_intin[0]=h; aes_intin[1]=f; r=aes(104,2,5,0,0);
  *a=aes_intout[1]; *b=aes_intout[2]; *c=aes_intout[3]; *d=aes_intout[4]; return r;
}
short wind_set(short h, short f, short a, short b, short c, short d)
{ aes_intin[0]=h; aes_intin[1]=f; aes_intin[2]=a; aes_intin[3]=b; aes_intin[4]=c; aes_intin[5]=d; return aes(105,6,1,0,0); }
short wind_set_str(short h, short f, const char *s)
{
  unsigned long p = (unsigned long)s;
  return wind_set(h, f, (short)(p >> 16), (short)(p & 0xffff), 0, 0);
}
short wind_find(short x, short y) { aes_intin[0]=x; aes_intin[1]=y; return aes(106,2,1,0,0); }
short wind_update(short m) { aes_intin[0]=m; return aes(107,1,1,0,0); }
short wind_calc(short t, short k, short x, short y, short w, short h, short *ox, short *oy, short *ow, short *oh)
{
  short r; aes_intin[0]=t; aes_intin[1]=k; aes_intin[2]=x; aes_intin[3]=y; aes_intin[4]=w; aes_intin[5]=h;
  r=aes(108,6,5,0,0); *ox=aes_intout[1]; *oy=aes_intout[2]; *ow=aes_intout[3]; *oh=aes_intout[4]; return r;
}
short graf_mkstate(short *mx, short *my, short *mb, short *ks)
{
  short r = aes(79,0,5,0,0);
  *mx=aes_intout[1]; *my=aes_intout[2]; *mb=aes_intout[3]; *ks=aes_intout[4];
  return r;
}
short graf_mouse(short m) { aes_intin[0]=m; aes_addrin[0]=0; return aes(78,1,1,1,0); }

/* --- VDI --- */
short v_opnvwk(short phys)
{
  int i;
  for (i = 0; i < 10; i++) vdi_intin[i] = 1;
  vdi_intin[10] = 2;
  vdi(100, 0, 11, phys);
  return vdi_contrl[6];
}
void v_clsvwk(short h) { vdi(101,0,0,h); }
void vs_clip(short h, short on, const short *r)
{ vdi_intin[0]=on; vdi_ptsin[0]=r[0]; vdi_ptsin[1]=r[1]; vdi_ptsin[2]=r[2]; vdi_ptsin[3]=r[3]; vdi(129,2,1,h); }
void vr_recfl(short h, const short *r)
{ vdi_ptsin[0]=r[0]; vdi_ptsin[1]=r[1]; vdi_ptsin[2]=r[2]; vdi_ptsin[3]=r[3]; vdi(114,2,0,h); }
void vsf_interior(short h, short s) { vdi_intin[0]=s; vdi(23,0,1,h); }
void vsf_color(short h, short c) { vdi_intin[0]=c; vdi(25,0,1,h); }
void vsf_perimeter(short h, short v) { vdi_intin[0]=v; vdi(104,0,1,h); }
void vswr_mode(short h, short m) { vdi_intin[0]=m; vdi(32,0,1,h); }
void vst_color(short h, short c) { vdi_intin[0]=c; vdi(22,0,1,h); }
void vst_alignment(short h, short hor, short ver) { vdi_intin[0]=hor; vdi_intin[1]=ver; vdi(39,0,2,h); }
void vst_effects(short h, short e) { vdi_intin[0]=e; vdi(106,0,1,h); }
void v_gtext(short h, short x, short y, const char *s)
{
  short n = 0;
  while (s[n] && n < 120) { vdi_intin[n] = (unsigned char)s[n]; n++; }
  vdi_ptsin[0]=x; vdi_ptsin[1]=y;
  vdi(8,1,n,h);
}
void v_box(short h, short x1, short y1, short x2, short y2)
{
  /* outline with v_pline */
  vdi_ptsin[0]=x1; vdi_ptsin[1]=y1; vdi_ptsin[2]=x2; vdi_ptsin[3]=y1;
  vdi_ptsin[4]=x2; vdi_ptsin[5]=y2; vdi_ptsin[6]=x1; vdi_ptsin[7]=y2;
  vdi_ptsin[8]=x1; vdi_ptsin[9]=y1;
  vdi(6,5,0,h);
}
