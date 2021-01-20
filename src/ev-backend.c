#include <stddef.h>
#include <stdint.h>

#include "ev-backend.h"

static const size_t backend_fd_offset = 196;

int tll_ev_backend_fd(struct ev_loop *l)
{
#ifdef __linux__
	return *(int *) (((uint8_t *) l) + backend_fd_offset);
#else
	return -1;
#endif
}
