/*
 * macOS COMPILE-ONLY shim for <linux/can.h>.
 *
 * SocketCAN only exists on Linux; this header provides just enough type and
 * constant definitions for sparkcan to COMPILE on macOS development machines
 * (socket(PF_CAN, ...) will fail at runtime, which is expected — CAN is only
 * used on the robot's Linux host). It is added to the include path via
 * if(APPLE) in CMakeLists.txt and is never installed or used on Linux.
 */

#ifndef MACOS_CAN_COMPAT__LINUX_CAN_H_
#define MACOS_CAN_COMPAT__LINUX_CAN_H_

#ifndef __APPLE__
#error "This shim is for macOS compile-only builds; use the real <linux/can.h> on Linux."
#endif

#include <net/if.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#ifndef PF_CAN
#define PF_CAN 29 /* Linux value; unusable at runtime on macOS */
#endif
#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

#ifndef SIOCGIFINDEX
#define SIOCGIFINDEX 0x8933 /* Linux value; ioctl will fail at runtime */
#endif

/* Linux spells the interface-index ifreq member ifr_ifindex; map it onto
 * macOS's generic integer union member so the sparkcan sources compile. */
#ifndef ifr_ifindex
#define ifr_ifindex ifr_ifru.ifru_intval
#endif

#define CAN_EFF_FLAG 0x80000000U /* extended frame format (29-bit id) */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

#define CAN_SFF_MASK 0x000007FFU
#define CAN_EFF_MASK 0x1FFFFFFFU
#define CAN_ERR_MASK 0x1FFFFFFFU

typedef uint32_t canid_t;

#define CAN_MAX_DLC 8
#define CAN_MAX_DLEN 8

struct can_frame
{
  canid_t can_id;  /* 32-bit CAN_ID + EFF/RTR/ERR flags */
  uint8_t can_dlc; /* frame payload length in bytes (0 .. CAN_MAX_DLEN) */
  uint8_t __pad;
  uint8_t __res0;
  uint8_t __res1;
  uint8_t data[CAN_MAX_DLEN] __attribute__((aligned(8)));
};

#define CAN_MTU (sizeof(struct can_frame))

struct can_filter
{
  canid_t can_id;
  canid_t can_mask;
};

struct sockaddr_can
{
  sa_family_t can_family;
  int can_ifindex;
  union {
    struct
    {
      canid_t rx_id, tx_id;
    } tp;
  } can_addr;
};

#endif /* MACOS_CAN_COMPAT__LINUX_CAN_H_ */
