#include <locale.h>
#include <ctype.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>

#define MAXDESC 2
char *path = "tic-tac-toe.server";

int main()
{       
    struct sockaddr_un address;
    char buff[BUFSIZ];
    int descriptor;
    fd_set rfdset;

    if ((descriptor = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("Can't create socket");
        return -1;
    }
    address.sun_family = AF_UNIX;
    memset(address.sun_path, 0, sizeof(address.sun_path));
    strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
    if (connect(descriptor, (struct sockaddr *) &address, sizeof(address)) == -1)
    {
        perror("error while connecting");
        return -1;
    }
    int alldescs[MAXDESC] = {STDIN_FILENO, descriptor};
    memset(buff, 0, BUFSIZ);
    ssize_t count;
    FD_ZERO(&rfdset);
    FD_SET(descriptor, &rfdset);
    FD_SET(STDIN_FILENO, &rfdset);
    while (1) {
        FD_SET(descriptor, &rfdset);
        FD_SET(STDIN_FILENO, &rfdset);
        select (descriptor+1, &rfdset, NULL, NULL, NULL);
        for (int i = 0; i < MAXDESC; i++) {
            if (FD_ISSET(alldescs[i], &rfdset)) {
                int socketfd = alldescs[i];
                int recv = read(socketfd, &buff, BUFSIZ);
                if (socketfd == descriptor) {
                        write(STDOUT_FILENO, &buff, BUFSIZ);
                } else {
                    write(descriptor, &buff, BUFSIZ);
                    }
            }
        }
    }
    close(descriptor);
    unlink(path);
    return 0;
}