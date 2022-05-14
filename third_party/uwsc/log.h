#ifndef _TLL_LIBUWSC_LOG_H
#define _TLL_LIBUWSC_LOG_H

#include <tll/logger.h>

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

tll_logger_t * uwsc_logger();
void uwsc_logger_ref();
void uwsc_logger_unref();

#ifdef __cplusplus
} // extern "C"
#endif//__cplusplus

#define uwsc_log_wrap(prio, fmt...)                      \
	do {                                             \
		tll_logger_t * l = uwsc_logger();        \
		if (l && prio > l->level)                \
			tll_logger_printf(l, prio, fmt); \
	} while (0)

#define log_debug(fmt...)     uwsc_log_wrap(TLL_LOGGER_DEBUG, fmt)
#define log_info(fmt...)      uwsc_log_wrap(TLL_LOGGER_INFO, fmt)
#define log_warn(fmt...)      uwsc_log_wrap(TLL_LOGGER_WARNING, fmt)
#define log_err(fmt...)       uwsc_log_wrap(TLL_LOGGER_ERROR, fmt)

#endif//_TLL_LIBUWSC_LOG_H
