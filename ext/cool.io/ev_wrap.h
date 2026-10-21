#define EV_STANDALONE              /* keeps ev from requiring config.h */

#ifdef _WIN32
#include "ruby.h"
#define EV_SELECT_IS_WINSOCKET 1   /* configure libev for windows select */
#define EV_USE_MONOTONIC 0
#define EV_USE_REALTIME 0
#define EV_WIN32_STAT_CODEPAGE CP_UTF8 /* Ruby paths are UTF-8. */
/* Close the evpipe sockets with closesocket() through Ruby. The CRT close()
 * only calls CloseHandle(), which leaves Winsock state behind. A socket later
 * imported from a WSAPROTOCOL_INFO with the same handle value inherits it, and
 * listen() fails with WSAEISCONN. */
#define EV_WIN32_HANDLE_TO_FD(handle) rb_w32_wrap_io_handle((HANDLE)(handle), 0)
#define EV_WIN32_CLOSE_FD(fd) rb_w32_close(fd)
#endif

#include "../libev/ev.h"

