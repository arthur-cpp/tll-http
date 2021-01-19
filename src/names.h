#ifndef _TLL_WS_NAMES_H
#define _TLL_WS_NAMES_H

#include <string_view>
#include <libwebsockets.h>

std::string_view lws_callback_name(lws_callback_reasons r);

#endif//_TLL_WS_NAMES_H
