| etv_critic (0x404) hook for the uIP accessory.
|
| When TOS hits a critical disk error (e.g. "drive A: not responding")
| it calls etv_critic from INSIDE a GEMDOS call; the AES handler then
| shows an alert and lets accessories run.  GEMDOS is not re-entrant,
| so the accessory must not call GEMDOS while this handler is active.
| This hook just counts how deep we are inside the handler.

        .text
        .globl  _uipacc_in_critic
        .globl  _uipacc_install_critic   | long uipacc_install_critic(void) - supervisor

        .dc.l   0x58425241              | "XBRA"
        .dc.l   0x75495041              | "uIPA"
old_critic:
        .dc.l   0
critic_hook:
        addq.w  #1,_uipacc_in_critic
        move.l  4(sp),-(sp)             | copy (err, drive) arguments
        move.l  old_critic(pc),a0
        jsr     (a0)
        addq.l  #4,sp
        subq.w  #1,_uipacc_in_critic    | does not touch d0 (return value)
        rts

_uipacc_install_critic:
        move.l  0x404.w,old_critic
        lea     critic_hook(pc),a0
        move.l  a0,0x404.w
        moveq   #0,d0
        rts

        .data
        .even
_uipacc_in_critic:
        .dc.w   0
