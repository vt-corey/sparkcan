/*
 * macOS COMPILE-ONLY shim for <linux/can/raw.h>. See ../can.h for rationale.
 */

#ifndef MACOS_CAN_COMPAT__LINUX_CAN_RAW_H_
#define MACOS_CAN_COMPAT__LINUX_CAN_RAW_H_

#ifndef __APPLE__
#error "This shim is for macOS compile-only builds; use the real <linux/can/raw.h> on Linux."
#endif

#include <linux/can.h>

#define SOL_CAN_BASE 100
#define CAN_RAW 1
#define SOL_CAN_RAW (SOL_CAN_BASE + CAN_RAW)

enum
{
  CAN_RAW_FILTER = 1,
  CAN_RAW_ERR_FILTER,
  CAN_RAW_LOOPBACK,
  CAN_RAW_RECV_OWN_MSGS,
  CAN_RAW_FD_FRAMES,
  CAN_RAW_JOIN_FILTERS,
};

#endif /* MACOS_CAN_COMPAT__LINUX_CAN_RAW_H_ */
