#pragma once

#include <switch/types.h>

Result custom_usbCommsInitialize(void);
void custom_usbCommsExit(void);
size_t custom_usbCommsRead(void *buffer, size_t size, u64 timeout = UINT64_MAX);
size_t custom_usbCommsWrite(const void *buffer, size_t size);
