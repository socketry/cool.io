
#undef accept

#undef bind

#undef connect

/* 
#undef FD_SET

#undef FD_CLR

#undef FD_ISSET
treated separately, below
*/



#undef getpeername

#undef getsockname

#undef getsockopt

#undef ioctlsocket

#undef listen

#undef recv


#undef recvfrom

#undef send

#undef sendto

#undef setsockopt

#undef shutdown

#undef socket

#undef gethostbyaddr


#undef gethostbyname

#undef gethostname

#undef getprotobyname

#undef getprotobynumber

#undef getservbyname

#undef getservbyport

#undef get_osfhandle
#undef getcwd

#undef getenv

#undef rename

#undef times

#undef select
#undef close

#undef read
#undef sleep

#undef write

#undef stat
#undef lstat

#define TO_SOCKET(s) s

void
rb_w32_fdset2(int fd, fd_set *set)
{
    unsigned int i;
    SOCKET s = TO_SOCKET(fd);

    for (i = 0; i < set->fd_count; i++) {
        if (set->fd_array[i] == s) {
            return;
        }
    }
    if (i == set->fd_count) {
        if (set->fd_count < FD_SETSIZE) {
            set->fd_array[i] = s;
            set->fd_count++;
        }
    }
}

void
rb_w32_fdclr2(int fd, fd_set *set)
{
    unsigned int i;
    SOCKET s = TO_SOCKET(fd);

    for (i = 0; i < set->fd_count; i++) {
        if (set->fd_array[i] == s) {
            while (i < set->fd_count - 1) {
                set->fd_array[i] = set->fd_array[i + 1];
                i++;
            }
            set->fd_count--;
            break;
        }
    }
}

int
rb_w32_fdisset2(int fd, fd_set *set)
{
    int ret;
    SOCKET s = TO_SOCKET(fd);
    if (s == (SOCKET)INVALID_HANDLE_VALUE)
        return 0;
    ret = __WSAFDIsSet(s, set);
    return ret;
}


#undef FD_SET
#define FD_SET(f, s)		rb_w32_fdset2(f, s)

#undef FD_CLR
#define FD_CLR(f, s)		rb_w32_fdclr2(f, s)

#undef FD_ISSET
#define FD_ISSET(f, s)		rb_w32_fdisset2(f, s)

