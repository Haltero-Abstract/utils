#ifndef __UIPACC_H__
#define __UIPACC_H__

#include <stdint.h>
#include <stdbool.h>

/* Network processing budget per accessory timer event */
#define UIPACC_MAX_ITER   200   /* max loop iterations while traffic flows */
#define UIPACC_IDLE_ITER  2     /* iterations when the network is idle     */

#define UIPACC_ERR_MINT    1
#define UIPACC_ERR_STIK    2
#define UIPACC_ERR_DRIVER  3

struct uipacc_status {
  bool active;
  bool static_ip;
  uint8_t ip[4];
  uint8_t mac[6];
};

void uipacc_set_config_path(const char* path);
long uipacc_check_env(void);       /* supervisor */
long uipacc_start(void);           /* supervisor */
long uipacc_stop(void);            /* supervisor */
long uipacc_step(void);            /* supervisor */
long uipacc_toggle_ipmode(void);   /* supervisor */
void uipacc_get_status(struct uipacc_status* st);

/* last line printed by the stack (printf is redirected here) */
extern char uipacc_lastlog[64];

#endif
