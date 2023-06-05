// SPDX-License-Identifier: BSD-3-Clause

/* SPDM proxy for spdm-emu (https://github.com/DMTF/spdm-emu.git).
 *
 * Copyright (C) 2022-2023 NVIDIA CORPORATION.
 * Copyright 2021-2022 DMTF. All rights reserved.
 */

#include <error.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "psc_mailbox.h"

#define DEFAULT_SPDM_PLATFORM_PORT 2323

#define SOCKET_SPDM_COMMAND_NORMAL 0x0001
#define SOCKET_SPDM_COMMAND_OOB_ENCAP_KEY_UPDATE 0x8001
#define SOCKET_SPDM_COMMAND_CONTINUE 0xFFFD
#define SOCKET_SPDM_COMMAND_SHUTDOWN 0xFFFE
#define SOCKET_SPDM_COMMAND_UNKOWN 0xFFFF
#define SOCKET_SPDM_COMMAND_TEST 0xDEAD

enum {
    SOCKET_TRANSPORT_TYPE_NONE,    /* raw packet */
    SOCKET_TRANSPORT_TYPE_MCTP,    /* MCTP payload */
    SOCKET_TRANSPORT_TYPE_PCI_DOE  /* DOE payload */
};

uint32_t m_use_transport_layer = SOCKET_TRANSPORT_TYPE_MCTP;

/**
 * Read number of bytes data in blocking mode.
 *
 * If there is no enough data in socket, this function will wait.
 * This function will return if enough data is read, or socket error.
 **/
bool read_bytes(const int socket, uint8_t *buffer,
                uint32_t number_of_bytes)
{
    int32_t result;
    uint32_t number_received;

    number_received = 0;
    while (number_received < number_of_bytes) {
        result = recv(socket, (char *)(buffer + number_received),
                      number_of_bytes - number_received, 0);
        if (result == -1) {
            printf("Receive error - %m\n");
            return false;
        }
        if (result == 0) {
            return false;
        }
        number_received += result;
    }
    return true;
}

bool read_data32(const int socket, uint32_t *data)
{
    bool result;

    result = read_bytes(socket, (uint8_t *)data, sizeof(uint32_t));
    if (!result) {
        return result;
    }
    *data = ntohl(*data);
    return true;
}

/**
 * Read multiple bytes in blocking mode.
 *
 * The length is presented as first 4 bytes in big endian.
 * The data follows the length.
 *
 * If there is no enough data in socket, this function will wait.
 * This function will return if enough data is read, or socket error.
 **/
bool read_multiple_bytes(const int socket, uint8_t *buffer,
                         uint32_t *bytes_received,
                         uint32_t max_buffer_length)
{
    uint32_t length;
    bool result;

    result = read_data32(socket, &length);
    if (!result) {
        return result;
    }

    *bytes_received = length;
    if (*bytes_received > max_buffer_length) {
        printf("buffer too small (0x%x). Expected - 0x%x\n",
               max_buffer_length, *bytes_received);
        return false;
    }
    if (length == 0) {
        return true;
    }
    result = read_bytes(socket, buffer, length);
    if (!result) {
        return result;
    }

    return true;
}

bool receive_platform_data(const int socket, uint32_t *command,
                           uint8_t *buffer,
                           uint32_t *size)
{
    bool result;
    uint32_t response;
    uint32_t transport_type;
    uint32_t bytes_received;

    result = read_data32(socket, &response);
    if (!result) {
        return result;
    }
    *command = response;

    result = read_data32(socket, &transport_type);
    if (!result) {
        return result;
    }
    if (transport_type != m_use_transport_layer) {
        printf("transport_type mismatch\n");
        return false;
    }

    bytes_received = 0;
    result = read_multiple_bytes(socket, buffer, &bytes_received,
                                 (uint32_t)*size);
    if (!result) {
        return result;
    }
    *size = bytes_received;

    return result;
}

/**
 * Write number of bytes data in blocking mode.
 *
 * This function will return if data is written, or socket error.
 **/
bool write_bytes(const int socket, const uint8_t *buffer,
                 uint32_t number_of_bytes)
{
    int32_t result;
    uint32_t number_sent;

    number_sent = 0;
    while (number_sent < number_of_bytes) {
        result = send(socket, (char *)(buffer + number_sent),
                      number_of_bytes - number_sent, 0);
        if (result == -1) {
            printf("Send error - %m\n");
            return false;
        }
        number_sent += result;
    }
    return true;
}

bool write_data32(const int socket, uint32_t data)
{
    data = htonl(data);
    return write_bytes(socket, (uint8_t *)&data, sizeof(uint32_t));
}

/**
 * Write multiple bytes.
 *
 * The length is presented as first 4 bytes in big endian.
 * The data follows the length.
 **/
bool write_multiple_bytes(const int socket, const uint8_t *buffer,
                          uint32_t bytes_to_send)
{
    bool result;

    result = write_data32(socket, bytes_to_send);
    if (!result) {
        return result;
    }

    result = write_bytes(socket, buffer, bytes_to_send);
    if (!result) {
        return result;
    }

    return true;
}

bool send_platform_data(const int socket, uint32_t command,
                        const uint8_t *buffer, uint32_t size)
{
    bool result;
    uint32_t request, transport_type;

    request = command;
    result = write_data32(socket, request);
    if (!result) {
        return result;
    }

    result = write_data32(socket, m_use_transport_layer);
    if (!result) {
        return result;
    }

    result = write_multiple_bytes(socket, buffer, size);
    if (!result) {
        return result;
    }

    return true;
}

bool platform_server(const int socket)
{
    uint32_t command, size;
    uint8_t buffer[0x1200 + 64];
    bool result;

    while (true) {
        size = sizeof(buffer);
        result = receive_platform_data(socket, &command,
                           buffer,
                           &size);
        if (!result || size > sizeof(buffer))
            continue;

        switch (command) {
        case SOCKET_SPDM_COMMAND_TEST:
            result = send_platform_data(socket,
                                        SOCKET_SPDM_COMMAND_TEST,
                                        (uint8_t *)"Server Hello!",
                                        sizeof("Server Hello!"));
            if (!result) {
                printf("send_platform_data Error - %m\n");
                return true;
            }
            break;

        case SOCKET_SPDM_COMMAND_OOB_ENCAP_KEY_UPDATE:
            result = send_platform_data(
                socket,
                SOCKET_SPDM_COMMAND_OOB_ENCAP_KEY_UPDATE, NULL,
                0);
            if (!result) {
                printf("send_platform_data Error - %m\n");
                return true;
            }
            break;

        case SOCKET_SPDM_COMMAND_SHUTDOWN:
            result = send_platform_data(
                socket, SOCKET_SPDM_COMMAND_SHUTDOWN, NULL, 0);
            if (!result) {
                printf("send_platform_data Error - %m\n");
                return true;
            }
            return true;

        case SOCKET_SPDM_COMMAND_CONTINUE:
            result = send_platform_data(
                socket, SOCKET_SPDM_COMMAND_CONTINUE, NULL, 0);
            if (!result) {
                printf("send_platform_data Error - %m\n");
                return true;
            }
            return true;

        case SOCKET_SPDM_COMMAND_NORMAL:
            result = psc_mailbox_send_msg(PSC_MBOX_SPDM_OPCODE, buffer, size);
            if (!result) {
                printf("psc_mailbox_send_msg failed\n");
                return true;
            }
            size = sizeof(buffer);
            result = psc_mailbox_recv_msg(PSC_MBOX_SPDM_OPCODE, buffer, &size);
            if (!result || !size) {
                printf("psc_mailbox_recv_msg failed\n");
                return true;
            }
            result = send_platform_data(
                socket, SOCKET_SPDM_COMMAND_NORMAL, buffer, size);
            if (!result) {
                printf("send_platform_data Error - %m\n");
            }
            break;

        default:
            printf("Unrecognized platform interface command %x\n",
                   command);
            result = send_platform_data(
                socket, SOCKET_SPDM_COMMAND_UNKOWN, NULL, 0);
            if (!result) {
                printf("send_platform_data Error - %m\n");
                return true;
            }
            return true;
        }
    }
}

bool create_socket(uint16_t port_number, int *listen_socket)
{
    struct sockaddr_in my_address;
    int32_t res;

    *listen_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == *listen_socket) {
        printf("Cannot create server listen socket %m\n");
        return false;
    }

    /* When the program stops unexpectedly the used port will stay in the TIME_WAIT
     * state which prevents other programs from binding to this port until a timeout
     * triggers. This timeout may be 30s to 120s. In this state the responder cannot
     * be restarted since it cannot bind to its port.
     * To prevent this SO_REUSEADDR is applied to the socket which allows the
     * responder to bind to this port even if it is still in the TIME_WAIT state.*/
    if (setsockopt(*listen_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        printf("Cannot configure server listen socket.  Error is %m\n");
        return false;
    }

    memset(&my_address, 0, sizeof(my_address));
    my_address.sin_port = htons((short)port_number);
    my_address.sin_family = AF_INET;

    res = bind(*listen_socket, (struct sockaddr *)&my_address,
               sizeof(my_address));
    if (res == -1) {
        printf("Bind error %m\n");
        close(*listen_socket);
        return false;
    }

    res = listen(*listen_socket, 3);
    if (res == -1) {
        printf("Listen error %m\n");
        close(*listen_socket);
        return false;
    }
    return true;
}

bool platform_server_routine(uint16_t port_number)
{
    int listen_socket, server_socket;
    struct sockaddr_in peer_address;
    bool result;
    uint32_t length;
    bool continue_serving;

    result = create_socket(port_number, &listen_socket);
    if (!result) {
        printf("Create platform service socket fail\n");
        return result;
    }

    do {
        printf("Platform server listening on port %d\n", port_number);

        length = sizeof(peer_address);
        server_socket =
            accept(listen_socket, (struct sockaddr *)&peer_address,
                   (socklen_t *)&length);
        if (server_socket == -1) {
            printf("Accept error %m\n");
            close(listen_socket);
            return false;
        }
        printf("Client accepted\n");

        continue_serving = platform_server(server_socket);
        close(server_socket);
    } while (continue_serving);

    close(listen_socket);
    return true;
}

int main(int argc, char *argv[])
{
    psc_mailbox_init();

    platform_server_routine(DEFAULT_SPDM_PLATFORM_PORT);

    return 0;
}
