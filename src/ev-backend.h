#ifndef _TLL_WS_EV_UTIL_H
#define _TLL_WS_EV_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

struct ev_loop;

int tll_ev_backend_fd(struct ev_loop *);

#ifdef __cplusplus
} // extern "C"
#endif

#endif//_TLL_WS_EV_UTIL_H
