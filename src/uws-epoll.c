#include "uws-epoll.h"

#ifdef LIBUS_USE_EPOLL
#define GET_READY_POLL(loop, index) (struct us_poll_t *) loop->ready_polls[index].data.ptr
#define SET_READY_POLL(loop, index, poll) loop->ready_polls[index].data.ptr = poll
#else
#define GET_READY_POLL(loop, index) (struct us_poll_t *) loop->ready_polls[index].udata
#define SET_READY_POLL(loop, index, poll) loop->ready_polls[index].udata = poll
#endif

int us_loop_step(struct us_loop_t * loop, int timeout_ms)
{
	/* Emit pre callback */
	us_internal_loop_pre(loop);

	/* Fetch ready polls */
#ifdef LIBUS_USE_EPOLL
	loop->num_ready_polls = epoll_wait(loop->fd, loop->ready_polls, 1024, timeout_ms);
#else
	loop->num_ready_polls = kevent(loop->fd, NULL, 0, loop->ready_polls, 1024, NULL);
#endif

	/* Iterate ready polls, dispatching them by type */
	for (loop->current_ready_poll = 0; loop->current_ready_poll < loop->num_ready_polls; loop->current_ready_poll++) {
		struct us_poll_t *poll = GET_READY_POLL(loop, loop->current_ready_poll);
		/* Any ready poll marked with nullptr will be ignored */
		if (poll) {
#ifdef LIBUS_USE_EPOLL
			int events = loop->ready_polls[loop->current_ready_poll].events;
			int error = loop->ready_polls[loop->current_ready_poll].events & (EPOLLERR | EPOLLHUP);
#else
			/* EVFILT_READ, EVFILT_TIME, EVFILT_USER are all mapped to LIBUS_SOCKET_READABLE */
			int events = LIBUS_SOCKET_READABLE;
			if (loop->ready_polls[loop->current_ready_poll].filter == EVFILT_WRITE) {
				events = LIBUS_SOCKET_WRITABLE;
			}
			int error = loop->ready_polls[loop->current_ready_poll].flags & (EV_ERROR | EV_EOF);
#endif
			/* Always filter all polls by what they actually poll for (callback polls always poll for readable) */
			events &= us_poll_events(poll);
			if (events || error) {
				us_internal_dispatch_ready_poll(poll, error, events);
			}
		}
	}
	/* Emit post callback */
	us_internal_loop_post(loop);
	return 0;
}

