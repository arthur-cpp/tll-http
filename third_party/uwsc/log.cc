#include "log.h"
#include <atomic>
#include <mutex>

static std::unique_ptr<tll::Logger> _uwsc_logger;
static unsigned _uwsc_logger_ref = 0;
static std::mutex _uwsc_logger_lock;

tll_logger_t * uwsc_logger() { return (tll_logger_t *) _uwsc_logger.get(); }
void uwsc_logger_ref()
{
	auto lock = std::unique_lock<std::mutex>(_uwsc_logger_lock);
	if (_uwsc_logger_ref++ == 0)
		_uwsc_logger.reset(new tll::Logger("uwsc"));
}

void uwsc_logger_unref()
{
	auto lock = std::unique_lock<std::mutex>(_uwsc_logger_lock);
	if (--_uwsc_logger_ref == 0)
		_uwsc_logger.reset();
}
