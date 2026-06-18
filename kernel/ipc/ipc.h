/**
 * novaOS — Inter-Process Communication
 * Message-passing IPC used between kernel, compositor, and apps.
 * Compositor and shell communicate entirely through IPC.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

struct Message {
    uint32_t sender_pid;
    uint32_t receiver_pid;
    uint32_t type;
    uint8_t  data[256];
    size_t   data_len;
};

namespace IPC {
    void init();
    int  send(uint32_t pid, const Message* msg);
    int  recv(Message* out_msg);          // Blocking
    int  try_recv(Message* out_msg);      // Non-blocking
    int  create_channel(const char* name);
    int  open_channel(const char* name);
}
