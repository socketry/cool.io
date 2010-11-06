#define EV_STANDALONE              /* keeps ev from requiring config.h */
#ifdef _WIN32
# define EV_SELECT_IS_WINSOCKET 1   /* configure libev for windows select */
# define FD_SETSIZE 2048 /* wishful thinking, as msvcrt6 [?] seems to only allow 512 fd's and 256 sockets max */
#endif

#include "../libev/ev.h"

