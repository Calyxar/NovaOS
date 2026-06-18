#include "ipc.h"
void IPC::init() {}
int IPC::send(uint32_t pid, const Message* msg) { return -1; }
int IPC::recv(Message* out_msg) { return -1; }
int IPC::try_recv(Message* out_msg) { return -1; }
int IPC::create_channel(const char* name) { return -1; }
int IPC::open_channel(const char* name) { return -1; }
