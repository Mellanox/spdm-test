// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause

/* PSC driver user space library.
 *
 * Copyright (C) 2022-2023 NVIDIA CORPORATION.
 */

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "psc_mailbox.h"

#define PSC_MBOX_BASE       0x12060000
#define PSC_MBOX_EXT_CTRL   0x4
#define PSC_MBOX_PSC_CTRL   0x8
#define PSC_MBOX_IN         0x800
#define PSC_MBOX_MAP_SIZE   0x10000

/** 16 IN/OUT parameters. IN: EXT -> PSC; OUT: PSC -> EXT. */
#define MBOX_BUF_NWORDS             16U

#define PSC_MAILBOX_TIMEOUT_USEC    1000000U

/* MB5 (ARM NON-SECURE base address) */
#define PSC_MBOX_EXT_CTRL_OFF       0x4U
#define   PSC_MBOX_EXT_CTRL_IN_VALID_MASK      0x1U
#define   PSC_MBOX_EXT_CTRL_OUT_DONE_MASK      0x10U
#define PSC_MBOX_PSC_CTRL_OFF       0x8U
#define   PSC_MBOX_PSC_CTRL_OUT_VALID_MASK     0x1U
#define PSC_MBOX_IN_OFF             0x800U
#define PSC_MBOX_OUT_OFF            0x1000U

void *psc_mbox_mmap;
int psc_mbox_fd;

static inline uint64_t psc_mailbox_get_usec(void)
{
    struct timeval t;

    gettimeofday(&t, NULL);

    return t.tv_sec * 1000000LL + t.tv_usec;
}

static inline void psc_mailbox_writel(uint32_t val, uint32_t offset)
{
    __sync_synchronize();

    if (psc_mbox_mmap)
        *(volatile uint32_t *)((uintptr_t)psc_mbox_mmap + offset) = val;
    else
        pwrite(psc_mbox_fd, &val, sizeof(val), offset);
}

static inline uint32_t psc_mailbox_readl(uint32_t offset)
{
    uint32_t val;

    if (psc_mbox_mmap)
        val = *(volatile uint32_t *)((uintptr_t)psc_mbox_mmap + offset);
    else
        pread(psc_mbox_fd, &val, sizeof(val), offset);

    __sync_synchronize();

    return val;
}

int psc_mailbox_init(void)
{
    int fd;

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("");
        return -1;
    }

    psc_mbox_mmap = (unsigned long *)mmap(NULL, PSC_MBOX_MAP_SIZE,
                                          PROT_READ | PROT_WRITE,
                                          MAP_SHARED | MAP_LOCKED,
                                          fd, PSC_MBOX_BASE);
    close(fd);

    if (psc_mbox_mmap != MAP_FAILED)
        return 0;
    psc_mbox_mmap = NULL;

    fd = open("/sys/devices/platform/MLNXBF3A:00/psc_mbox", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("");
        return -1;
    }

    psc_mbox_fd = fd;

    return 0;
}

/* Mark it done receiving the message from PSC. */
static inline void psc_mailbox_out_done(void)
{
    uint32_t ext_ctrl;

    ext_ctrl = psc_mailbox_readl(PSC_MBOX_EXT_CTRL_OFF);
    ext_ctrl |= PSC_MBOX_EXT_CTRL_OUT_DONE_MASK;
    psc_mailbox_writel(ext_ctrl, PSC_MBOX_EXT_CTRL_OFF);
}

/* Check if there is message from PSC. */
static inline bool psc_mailbox_out_valid(void)
{
    uint32_t psc_ctrl;

    psc_ctrl = psc_mailbox_readl(PSC_MBOX_PSC_CTRL_OFF);

    return (psc_ctrl & PSC_MBOX_PSC_CTRL_OUT_VALID_MASK) ? true : false;
}

/*
 * Send mailbox message
 *
 * This API sends a large message in segments with header format
 * psc_mailbox_seg_hdr_t in each one.
 */
bool psc_mailbox_send_msg(uint32_t opcode, uint16_t context_id,
                          const uint8_t *buf, uint32_t len)
{
    psc_mailbox_seg_hdr_t hdr;
    bool status = false;

    if ((NULL != buf) && (len > 0U)) {
        uint64_t t0 = psc_mailbox_get_usec(), t1;
        uint32_t i, ext_ctrl, remaining = len, cur_len, data;

        /* Try to send all data in a loop. */
        while (remaining != 0U) {
            /* Timeout if exceeding the limit. */
            t1 = psc_mailbox_get_usec();
            if (t1 - t0 > PSC_MAILBOX_TIMEOUT_USEC) {
                printf("Tx timeout\n");
                status = false;
                break;
            }

            /*
             * Check the pending write state. This bit should be cleared by HW
             * once PSC received / processed the last message.
             */
            ext_ctrl = psc_mailbox_readl(PSC_MBOX_EXT_CTRL_OFF);
            if (ext_ctrl & PSC_MBOX_EXT_CTRL_IN_VALID_MASK) {
                usleep(1000);
                continue;
            }

            /* word0: opcode */
            psc_mailbox_writel(opcode, PSC_MBOX_IN_OFF);

            /* word1: more_data(1B) + cur_len(1B) + offset(2B) */
            hdr.words[1] = 0U;
            hdr.ctx_id = (uint8_t)context_id;
            cur_len = (remaining > (MBOX_BUF_NWORDS - 2U) * 4U) ?
                ((MBOX_BUF_NWORDS - 2U) * 4U) : remaining;
            hdr.cur_len = cur_len & 0xFFU;
            hdr.offset = (len - remaining) & 0xFFFFU;
            if (cur_len != remaining) {
                hdr.more = 0x1U;
            }
            psc_mailbox_writel(hdr.words[1], PSC_MBOX_IN_OFF + 4U);

            /* word2~15: data */
            for (i = 0U; i < cur_len / 4U; i++) {
                ((uint8_t *)&data)[0] = *buf++;
                ((uint8_t *)&data)[1] = *buf++;
                ((uint8_t *)&data)[2] = *buf++;
                ((uint8_t *)&data)[3] = *buf++;
                psc_mailbox_writel(data, PSC_MBOX_IN_OFF + 8U + (0x4U * i));
            }

            /* Write leftover bytes. */
            if (cur_len & 0x3U) {
                data = 0U;
                for (i = 0U; i < (cur_len & 0x3U); i++) {
                    ((uint8_t *)&data)[i] = *buf++;
                }
                psc_mailbox_writel(data,
                               PSC_MBOX_IN_OFF + 8U + (cur_len & ~0x3U));
            }

            remaining -= cur_len;

            /* This is for Linux. */
            if (psc_mbox_mmap) {
                msync(psc_mbox_mmap + PSC_MBOX_IN_OFF, MBOX_BUF_NWORDS * 4U,
                      MS_SYNC | MS_INVALIDATE);
            }

            /* Submit this message. */
            ext_ctrl = psc_mailbox_readl(PSC_MBOX_EXT_CTRL_OFF);
            ext_ctrl |= PSC_MBOX_EXT_CTRL_IN_VALID_MASK;
            psc_mailbox_writel(ext_ctrl, PSC_MBOX_EXT_CTRL_OFF);

            /* This is for Linux. */
            if (psc_mbox_mmap) {
                msync(psc_mbox_mmap + PSC_MBOX_IN_OFF, MBOX_BUF_NWORDS * 4U,
                      MS_SYNC | MS_INVALIDATE);
            }
        }

        if (!remaining) {
            status = true;
        }
    }

    return status;
}

/*
 * Receive mailbox message
 *
 * This API receives a large message in segments with header format
 * psc_mailbox_seg_hdr_t in each one.
 */
bool psc_mailbox_recv_msg(uint32_t opcode, uint16_t *context_id,
                          uint8_t *buf, uint32_t *len)
{
    psc_mailbox_seg_hdr_t hdr = { .more = 1U };
    bool status = false;

    if ((NULL != buf) && (len != NULL) && (*len > 0U)) {
        uint32_t i, data, offset = 0U;
        uint64_t t0 = psc_mailbox_get_usec(), t1;

        *context_id = 0xFFFF;

        /* Receive message in a loop. */
        do {
            /* Timeout if exceeding the time limit. */
            t1 = psc_mailbox_get_usec();
            if (t1 - t0 > PSC_MAILBOX_TIMEOUT_USEC) {
                printf("Rx timeout\n");
                status = false;
                break;
            }

            /* Check data availablity. */
            if (!psc_mailbox_out_valid()) {
                usleep(1000);
                continue;
            }

            /* word0: opcode */
            hdr.words[0] = psc_mailbox_readl(PSC_MBOX_OUT_OFF);

            /* Don't continue if opcode has changed. */
            if (hdr.words[0] != opcode) {
                printf("opcode changed\n");
                status = false;
                break;
            }

            /* word1: more_data(1B) + cur_len(1B) + offset(2B) */
            hdr.words[1] = psc_mailbox_readl(PSC_MBOX_OUT_OFF + 4U);

            /*
             * Sanity check. The following cases are considered invalid:
             * - current length in this message is 0;
             * - current length is more than max length of this message;
             * - total length exceeds the buffer length;
             * - 'more' flag is set but current length is not time of 4;
             */
            if (*context_id == 0xFFFF) {
                *context_id = hdr.ctx_id;
            }
            if ((hdr.ctx_id != *context_id) ||
                (hdr.cur_len == 0U) ||
                (hdr.cur_len > (MBOX_BUF_NWORDS - 2U) * 4U) ||
                (offset + hdr.cur_len > *len) ||
                (hdr.more && (hdr.cur_len & 0x3U))) {
                psc_mailbox_out_done();
                status = false;
                printf("sanity check failed\n");
                break;
            }

            /*
             * The accumulated offset should match the 'offset' value in the
             * message. If not match, terminate and drop the message if the
             * offset in the message is not 0 (offset 0 is another new message).
             */
            if (offset != hdr.offset) {
                if (hdr.offset != 0) {
                    psc_mailbox_out_done();
                }
                printf("offset mismatch\n");
                status = false;
                break;
            }

            /* word2~15: data */
            for (i = 0U; i < hdr.cur_len / 4U; i++) {
                data = psc_mailbox_readl(
                                   PSC_MBOX_OUT_OFF + 8U + (0x4U * i));
                *buf++ = ((uint8_t *)&data)[0];
                *buf++ = ((uint8_t *)&data)[1];
                *buf++ = ((uint8_t *)&data)[2];
                *buf++ = ((uint8_t *)&data)[3];
            }

            /* Read the remaining data. */
            if (hdr.cur_len & 0x3U) {
                data = psc_mailbox_readl(
                               PSC_MBOX_OUT_OFF + 8U + (hdr.cur_len & ~0x3U));
                for (i = 0U; i < (hdr.cur_len & 0x3U); i++) {
                    *buf++ = ((uint8_t *)&data)[i];
                }
            }

            offset += hdr.cur_len;

            /* Finished this segment. */
            psc_mailbox_out_done();

            if (!hdr.more) {
                *len = offset;
                status = true;
                break;
            }
        } while (hdr.more);
    }

    return status;
}
