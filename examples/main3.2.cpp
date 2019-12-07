#define _XOPEN_SOURCE 600
#define SO_REUSEPORT    0x0200          /* allow local address & port reuse */
#include <sys/socket.h>
#include <sys/errno.h>
#include <netdb.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include "./taskruntime3.1.h"
//#include "./taskruntime3.2.h"

void silk__r(const int socket) {
    char buf[1024];
    int n;
         
    while (1) {
        n = silk__read_async(socket, buf, 1024);

        if (n <= 0) {
            close(socket);

            return;
        }
  
        printf("[%d] silk__r(%d) [%d] %s\n", silk__current_worker_id, socket, n, buf);
    }
}

void silk__a(const int listensocket) {
    int s;
    struct sockaddr_storage addr;
    socklen_t socklen = sizeof(addr);

    while(1) {
        s = silk__accept_async(listensocket, (struct sockaddr*) &addr, &socklen);

        if ( s != -1 ) {
            silk__spawn(silk__coro silk__r, 32768, 1, s);
        }
    }
}

int main() {
    silk__init_pool(silk__schedule, silk__makeuwcontext);

    struct addrinfo hints, *ser;

    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, "3491", & hints, &ser);

    int listensockfd = socket(ser->ai_family, ser->ai_socktype, ser->ai_protocol);

    fcntl(listensockfd, F_SETFL, fcntl(listensockfd, F_GETFL, 0) | O_NONBLOCK);

    int yes = 1;
    setsockopt(listensockfd, SOL_SOCKET, SO_REUSEPORT, & yes, sizeof(int));

    bind(listensockfd, ser-> ai_addr, ser-> ai_addrlen);

    listen(listensockfd, SOMAXCONN);

    kq = kqueue();

    silk__spawn(silk__coro silk__a, 32768, 1, listensockfd);

    struct kevent evSet;
    struct kevent evList[1024];

    int n = 0;

    while (1) {
        int nev = kevent(kq, NULL, 0, evList, 1024, NULL); //io poll...

        for (int i = 0; i < nev; i++) {  //run pending...
            if (evList[i].ident == listensockfd) {
                silk__schedule((silk__coro_frame*) evList[i].udata); //silk__resume((silk__coro_frame*) evList[i].udata);
            }  else if (evList[i].filter == EVFILT_READ) {
                silk__io_read_frame* frame = (silk__io_read_frame*) evList[i].udata;

                memset(frame->buf, 0, frame->nbytes);

                frame->n = evList[i].flags & EV_EOF ? 0 : read(evList[i].ident, frame->buf, frame->nbytes);

                silk__resume(frame->coro_frame);
            }
        }
    }

    return 0;
}