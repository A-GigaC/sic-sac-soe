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
#include <errno.h>
#include <string.h>

#define handle_error(msg) \
do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_CONNECTIONS 2
#define PATH "echo.server"
// game
#define CROSS 1
#define ZERO -1
#define NONE 0

char *x = "\\ / X / \\";
char *o = "0000 0000";

int field[3][3] = {
        {NONE, NONE, NONE},
        {NONE, NONE, NONE},
        {NONE, NONE, NONE}
};

char simple_matrix[3][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},  
    {'7', '8', '9'}
};

void draw(int fp, int sp, char * buf);
void simpledraw(int fp, int sp);
void clear(char * outbuf, int fp, int sp);
int wincon(int pl);

// end game
int game(int sfd);
 
int main() {
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sun_family = AF_UNIX;
    strncpy(my_addr.sun_path, PATH, sizeof(my_addr.sun_path) - 1);

    if (bind(sfd, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1)
        handle_error("bind");

    if (listen(sfd, MAX_CONNECTIONS) == -1)
        handle_error("listen");
    
    game(sfd);
    unlink(PATH);
    close(sfd);

    return 0;
}

int game(int sfd) {
    socklen_t sockaddrSize = sizeof(struct sockaddr);
    struct sockaddr_un client_addr;
    
    int fp, sp;
    fp = accept(sfd, (struct sockaddr *) &client_addr, &sockaddrSize);
    sp = accept(sfd, (struct sockaddr *) &client_addr, &sockaddrSize);
    
    fd_set rfdset;
    int fds[2] = {fp, sp};
    FD_ZERO(&rfdset);
    FD_SET(fp, &rfdset);
    FD_SET(sp, &rfdset);

    char inpbuf[2] = {0};
    char outbuf[256] = {0};
    char c;
    int cp, cpf, win;
    int status = 0;
    int line, column;

    simpledraw(fp, sp);

    cp = status % 2 + 1;
    cpf = (status % 2 + 1) % 2 ? fp : sp;
    printf("now cpf is %d\n", cpf);
    while ( 1 ) {

        if (status > 8) {
            sprintf(outbuf, "\nBYE\n");
            write(fp, &outbuf, 6);
            write(sp, &outbuf, 6); 

            close(fp);
            close(sp);

            return 0;
        }

        FD_ZERO(&rfdset);
        FD_SET(fp, &rfdset);
        FD_SET(sp, &rfdset);
        select(sp+1, &rfdset, NULL, NULL, NULL);

        for (int i = 0; i < 2; i++) {
            if (FD_ISSET(fds[i], &rfdset)) {
                read(fds[i], &inpbuf, 1);
                c = inpbuf[0];

                // quit from game
                if (c == 'q') {
                    if (status == 0) {
                        sprintf(outbuf, "\n\tDraw\n");
                        write(fp, &outbuf,8);
                        write(sp, &outbuf, 8); 
                        status = 111;
                        break;
                    } else {
                        sprintf(outbuf, "\n%d player wins\n", cp);
                        write(fp, &outbuf, 17);
                        write(sp, &outbuf, 17);
                        status = cp > 1 ? 110 : 101;
                        printf("%d\n", status);
                        break; 
                    }
                }
                // if user, that make input, is current
                if (fds[i] == cpf) {
                    
                    if (c >= '1' && c <= '9') {
                        int nc = c - '1';
                        line = nc / 3;
                        column = nc % 3;
                        if (field[line][column] == 0) {
                            field[line][column] = status % 2 != 0 ? ZERO : CROSS;

                            sprintf(outbuf, "----- step %d ----\n", status + 1);
                            write(fp, &outbuf, 20);
                            write(sp, &outbuf, 20);
                            simpledraw(fp, sp);
                            status++;                        
                            cp = status % 2 + 1;
                            cpf = (status % 2 + 1) % 2 ? fp : sp;
                            printf("cpf is %d\n", cpf);
                        } else {
                            sprintf(outbuf, "incorrect input\n");
                            write(cpf, &outbuf, 17);
                            continue;
                        }
                        if ((win = wincon(1)) != 0) {
                            sprintf(outbuf, "\n1st player wins\n");
                            write(fp, &outbuf, 18);
                            write(sp, &outbuf, 18); 

                            status = 101;
                            break;
                        } else if ((win = wincon(2)) != 0) {
                            sprintf(outbuf, "\n2nd player wins\n");
                            write(fp, &outbuf, 20);
                            write(sp, &outbuf, 20); 

                            status = 110;
                            break;
                        }

                    }
                }

            }
        }

    }

    return 0;
}

void simpledraw(int fp, int sp) {
    char buf[30];
    // matrix updating
    for (int line = 0; line < 3; line++) {
        for (int column = 0; column < 3; column++) {
            if (field[line][column] == CROSS) {
                simple_matrix[line][column] = 'x';
            } else  if (field[line][column] == ZERO) {
                simple_matrix[line][column] = '0';
            }
        }
    }
    int ctr = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j ++) {
            buf[ctr++] = simple_matrix[i][j];
            if (j < 2) { buf[ctr++] = '|'; }
        }
        buf[ctr++] = '\n';
        buf[ctr++] = '-';
        buf[ctr++] = '+';
        buf[ctr++] = '-';
        buf[ctr++] = '+';
        buf[ctr++] = '-';
        buf[ctr++] = '\n';
    }
    write(fp, &buf, 30);
    write(sp, &buf, 30);    
}


int wincon(int pl) {
    int target, line, column, eq, win = 0;

    target = pl == 1 ? CROSS : ZERO;

    // horizontal
    for (line = 0; line < 3; line++) {
        eq = 0;
        for (column = 0; column < 3; column++) {
            if (field[line][column] == target) {
                ++eq;
            }
        }
        if (eq == 3) {
            win = 1;
        }
    }
    // vertical
    for (column = 0; column < 3; column++) {
        eq = 0;
        for (line = 0; line < 3; line++) {
            if (field[line][column] == target) {
                ++eq;
            }
        }
        if (eq == 3) {
            win = 1;
        }
    }
    // diagonal R
    eq = 0;
    for (line = 0, column = 0; column < 3 && line < 3; column++, line++) {
        if (field[line][column] == target) {
            ++eq;
        }
        if (eq == 3) {
            win = 1;
        }
    }
    // diagonal L
    eq = 0;
    for (line = 0, column = 2; column >= 0 && line < 3; column--, line++) {
        if (field[line][column] == target) {
            ++eq;
        }
        if (eq == 3) {
            win = 1;
        }
    }
    return win;
}