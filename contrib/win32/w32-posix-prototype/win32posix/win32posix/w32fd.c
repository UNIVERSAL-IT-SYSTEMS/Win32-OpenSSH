#include "w32posix.h"
#include "w32fd.h"
#include <stdarg.h>

struct w32fd_table {
    w32_fd_set occupied;
    struct w32_io* w32_ios[MAX_FDS];
};

struct w32fd_table fd_table;

int fd_table_initialize() {
    memset(&fd_table, 0, sizeof(fd_table));
    //set stdin, stdout and stderr
    return 0;
}

int fd_table_get_min_index() {
    int min_index = 0;
    unsigned char* bitmap = fd_table.occupied.bitmap;
    unsigned char tmp;

    while (*bitmap == 0xff)
    {
        bitmap++;
        min_index += 8;
        if (min_index >= MAX_FDS)
            return -1;
    }

    tmp = *bitmap;

    while (tmp & 0x80)
    {
        tmp <<= 1;
        min_index++;
    }

    return min_index;
}

void fd_table_set(struct w32_io* pio, int index) {

    fd_table.w32_ios[index] = pio;
    pio->table_index = index;
    FD_SET(index, &(fd_table.occupied));
}

void fd_table_clear(int index)
{
    fd_table.w32_ios[index]->table_index = -1;
    fd_table.w32_ios[index] = NULL;
    FD_CLR(index, &(fd_table.occupied));
}

void w32posix_initialize() {
    socketio_initialize();
}

void w32posix_done() {
    socketio_done();
}

BOOL w32_io_is_blocking(struct w32_io* pio)
{
    return (pio->fd_status_flags & O_NONBLOCK) ? TRUE : FALSE;
}

int w32_socket(int domain, int type, int protocol) {
    int min_index = fd_table_get_min_index();
    struct w32_io* pio = NULL;

    if (min_index == -1)
    {
        return -1;
    }

    pio = socketio_socket(domain, type, protocol);
    if (!pio) {
        return -1;
    }

    fd_table_set(pio, min_index);
    return min_index;
}

int w32_accept(int fd, struct sockaddr* addr, int* addrlen)
{
    int min_index = fd_table_get_min_index();
    struct w32_io* pio = NULL;

    if (min_index == -1)
    {
        return -1;
    }

    pio = socketio_accept(fd_table.w32_ios[fd], addr, addrlen);
    if (!pio) {
        return -1;
    }

    fd_table_set(pio, min_index);
    return min_index;
}

int w32_setsockopt(int fd, int level, int optname, const char* optval, int optlen) {
    return socketio_setsockopt(fd_table.w32_ios[fd], level, optname, optval, optlen);
}

int w32_getsockopt(int fd, int level, int optname, char* optval, int* optlen) {
    return socketio_getsockopt(fd_table.w32_ios[fd], level, optname, optval, optlen);
}

int w32_getsockname(int fd, struct sockaddr* name, int* namelen) {
    return socketio_getsockname(fd_table.w32_ios[fd], name, namelen);
}

int w32_getpeername(int fd, struct sockaddr* name, int* namelen) {
    return socketio_getpeername(fd_table.w32_ios[fd], name, namelen);
}

int w32_listen(int fd, int backlog) {
    return socketio_listen(fd_table.w32_ios[fd], backlog);
}

int w32_bind(int fd, const struct sockaddr *name, int namelen) {
    return socketio_bind(fd_table.w32_ios[fd], name, namelen);
}

int w32_connect(int fd, const struct sockaddr* name, int namelen) {
    return socketio_connect(fd_table.w32_ios[fd], name, namelen);
}

int w32_shutdown(int fd, int how) {
    return socketio_shutdown(fd_table.w32_ios[fd], how);
}

int w32_close(int fd) {
    struct w32_io* pio = fd_table.w32_ios[fd];

    fd_table_clear(pio->table_index);
    if ((pio->type == LISTEN_FD) || (pio->type == SOCK_FD)) {
        socketio_close(pio);
        return 0;
    }
    else
        return -1;
}

int w32_fcntl(int fd, int cmd, ... /* arg */) {
    va_list valist;
    va_start(valist, cmd);

    switch (cmd){
    case F_GETFL:
        return fd_table.w32_ios[fd]->fd_status_flags;
    case F_SETFL:
        fd_table.w32_ios[fd]->fd_status_flags = va_arg(valist, int);
        return 0;
    case F_GETFD:
        return fd_table.w32_ios[fd]->fd_flags;
        break;
    case F_SETFD:
        fd_table.w32_ios[fd]->fd_flags = va_arg(valist, int);
        return 0;
    }
}
