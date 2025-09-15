#include "log.h"
#include <atomic>
#include <mutex>

static tll_logger_t * _uwsc_logger;
static unsigned _uwsc_logger_ref = 0;
static std::mutex _uwsc_logger_lock;

tll_logger_t * uwsc_logger() { return _uwsc_logger; }

void uwsc_logger_ref()
{
	auto lock = std::unique_lock<std::mutex>(_uwsc_logger_lock);
	if (_uwsc_logger_ref++ == 0)
		_uwsc_logger = tll_logger_new("uwsc", -1);
}

void uwsc_logger_unref()
{
	auto lock = std::unique_lock<std::mutex>(_uwsc_logger_lock);
	if (--_uwsc_logger_ref == 0) {
		tll_logger_free(_uwsc_logger);
		_uwsc_logger = nullptr;
	}
}
