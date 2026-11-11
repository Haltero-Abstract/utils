/* Force-included (-include accshim.h) in the accessory build only.
 * A desk accessory must never write to the TOS console (it would draw
 * over the GEM desktop), so every printf() of the uIP stack is captured
 * into a small buffer that the accessory window displays. */
#ifndef __ACCSHIM_H__
#define __ACCSHIM_H__
int uipacc_log(const char *fmt, ...);
#define printf uipacc_log
#endif
