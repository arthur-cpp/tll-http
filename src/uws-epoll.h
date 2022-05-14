#ifndef _TLL_UWS_EPOLL_H
#define _TLL_UWS_EPOLL_H

#define LIBUS_USE_EPOLL
#include "libusockets.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

#include "internal/internal.h"
#include "internal/eventing/epoll_kqueue.h"

int us_loop_step(struct us_loop_t * loop, int timeout_ms);

#ifdef __cplusplus
} // extern "C"
#endif//__cplusplus

#endif//_TLL_UWS_EPOLL_H
