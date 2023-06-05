/* SPDX-License-Identifier: GPL-2.0-only or BSD-3-Clause */

/* PSC driver user space library header file.
 *
 * Copyright (c) 2023 NVIDIA Corporation.
 */

#ifndef _PSC_MAILBOX_H_
#define _PSC_MAILBOX_H_

#define PSC_MBOX_SPDM_OPCODE     0x5350444dU

int psc_mailbox_init(void);

bool psc_mailbox_send_msg(uint32_t opcode, const uint8_t *buf, uint32_t len);

bool psc_mailbox_recv_msg(uint32_t opcode, uint8_t *buf, uint32_t *len);

#endif /* _PSC_MAILBOX_H_ */
