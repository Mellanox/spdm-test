/* SPDX-License-Identifier: GPL-2.0-only or BSD-3-Clause */

/* PSC driver user space library header file.
 *
 * Copyright (c) 2023 NVIDIA Corporation.
 */

#ifndef _PSC_MAILBOX_H_
#define _PSC_MAILBOX_H_

/* Mailbox message opcode for SPDM. */
#define PSC_MBOX_SPDM_OPCODE     0x5350444dU

/**
 * Segment header when splitting large message into multiple segments and
 * send each of them over mailbox.
 */
typedef union psc_mailbox_seg_hdr {
    struct {
        uint32_t opcode;        /* message opcode */
        uint16_t offset;        /* offset of this block within the message */
        uint8_t cur_len;        /* actual length in this block */
        uint8_t more : 1;       /* flag to indicate more blocks */
        uint8_t ctx_id : 3;     /* context id */
        uint8_t rsvd : 4;       /* reserved */
    };
    uint32_t words[2];
} psc_mailbox_seg_hdr_t;

/* Initialize mailbox transport. */
int psc_mailbox_init(void);

/*
 * Send mailbox message
 *
 * This API sends a large message in segments with header format
 * psc_mailbox_seg_hdr_t in each one.
 *
 * opcode: mailbox opcode
 * context_id: spdm session identifier
 * buf: message buffer pointer
 * len: message length
 *
 */
bool psc_mailbox_send_msg(uint32_t opcode, uint16_t context_id,
                          const uint8_t *buf, uint32_t len);

/*
 * Receive mailbox message
 *
 * This API receives a large message in segments with header format
 * psc_mailbox_seg_hdr_t in each one.
 *
 * opcode: mailbox opcode
 * context_id: spdm session identifier
 * buf: message buffer pointer
 * len: message length
 *
 */
bool psc_mailbox_recv_msg(uint32_t opcode, uint16_t *context_id,
                          uint8_t *buf, uint32_t *len);

#endif /* _PSC_MAILBOX_H_ */
