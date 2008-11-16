#define EV_STANDALONE              /* keeps ev from requiring config.h */
#ifdef _WIN32
# define EV_SELECT_IS_WINSOCKET 1   /* configure libev for windows select */
#endif

#include "../libev/ev.h"

