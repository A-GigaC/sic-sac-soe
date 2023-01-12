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

#define MAX_CONNECTIONS 256
#define PATH "tic-tac-toe.server"

#define CROSS 1
#define ZERO -1
#define NONE 0


int not_in(int elem, int *array);

int accept_connections(int sfd);
int reg_cfd(int cfd, int * all_descs, int len);

void close_server(int sfd, int * all_dedscs); 

typedef struct game_session {
    int fp;
    int sp;
    int turn;
    int status;
    int field_statement[3][3];
    char field[3][3];
} game_session;
 
void create_session (int p, game_session session, game_session * sessions);
void draw(game_session session, game_session * sessions);
int wincon(game_session session);


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
    
    accept_connections(sfd);

    return 0;
}

int accept_connections(int sfd) {

    fd_set rfdset;
    FD_ZERO(&rfdset);

    int all_descs[MAX_CONNECTIONS] = {0};

    int in_party[MAX_CONNECTIONS] = {0};
    game_session sessions[MAX_CONNECTIONS/2] = {0};
    socklen_t sockaddrSize = sizeof(struct sockaddr);
    struct sockaddr_un client_addr;

    char inpbuf[256] = {0};
    char outbuf[256] = {0};

    while (1) {

        FD_ZERO(&rfdset);
        FD_SET(sfd, &rfdset);

        FD_SET(STDIN_FILENO, &rfdset);

        int maxfd = sfd;

        for (int i = 0; i < MAX_CONNECTIONS; ++i) {
            int cfd = all_descs[i];
            if (cfd > 0) { FD_SET(cfd, &rfdset); }
            if (cfd > maxfd) { maxfd = cfd; }
        }

        int events = select(maxfd+1, &rfdset, NULL, NULL, NULL);
        if (events == -1) { perror("select()"); }

        /* console interaction handling */
        if (FD_ISSET(STDIN_FILENO, &rfdset)) {
            read(STDIN_FILENO, &inpbuf, 1);
            if (inpbuf[0] == 'q') { 
                close_server(sfd, &all_descs);
                return 1;
            }
        }
        /* accept new connection */
        if (FD_ISSET(sfd, &rfdset)) {
            
            int cfd = accept(sfd, (struct sockaddr *) &client_addr, &sockaddrSize);
            if (cfd == -1) { 
                fprintf(stderr, " Failed to accpet a socket\n", strerror(errno));
                continue;
            }
            if (cfd > 0) { 
                int res = reg_cfd(cfd, &all_descs, sizeof(all_descs)); 
                if (res == -1) { fprintf(stderr, "too many connections\n"); }
                else { fprintf(stderr, "socket registred\n"); }
            }
        }

        /* user interaction */
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            // // /* delete bad descriptors from descriptors array */ 
            // if (FD_ISSET(all_descs[i], &rfdset) == 0) {
            //     for (int k = i; k < sizeof(all_descs) - 1;  k++) { 
                    
            //         for (int it = 0; it < sizeof(in_party); it++) {
            //             if (in_party[it] == all_descs[i]) {
            //                 in_party[it] = 0;
            //                 break;
            //             }
            //         }
            //         all_descs[k] = all_descs[k+1];
            //     }
            // }
            /* matchmacking */
            int p = all_descs[i];
            if (p == 0) { continue; }
            int not_in_game = 1;
            if (not_in(p, &in_party)) {
                for (int k = 0; k < MAX_CONNECTIONS / 2; k ++) {
                    game_session session = sessions[k];
                    if (session.sp == NULL) {
                        if (session.fp == NULL) { 
                            create_session(p, session, sessions);
                            sprintf(&outbuf, "wait the secondplayer\n");
                            write(p, &outbuf, 22);
                            }
                        else { 
                            session.sp = p;
                            sprintf(&outbuf, "you are secondplayer\n");
                            write(p, &outbuf, 21);
                            session.status = 1;
                            draw(session, sessions);
                            sessions[k] = session;
                            }
                        
                        for (int ip = 0; ip < sizeof(in_party); ip++) {
                            if (in_party[i] == 0) {
                                in_party[i] = p;
                                break;
                            } 
                        }
                        not_in_game = 0;
                        break;
                    }
                }
            }    
            else if (FD_ISSET(p, &rfdset)){
                for (int pi = 0; pi < MAX_CONNECTIONS / 2; pi++) {
                    game_session session = sessions[pi];
                    if ((session.fp == p || session.sp == p) && session.status > 0 && session.status < 10) {
                        int number_in_session = session.fp == p ? 1 : 2;
                        read(p, &inpbuf, 1);
                        char c = inpbuf[0];
                        // quit from game
                        if (c == 'q') {
                            if (session.status == 1) {
                                sprintf(outbuf, "\nDraw\n");
                                write(session.fp, &outbuf,6);
                                write(session.sp, &outbuf, 6); 
                                session.status = 111;
                            } else {
                                sprintf(outbuf, "\nthe %d player gave up\n", number_in_session);
                                write(session.fp, &outbuf, 24);
                                write(session.sp, &outbuf, 24);
                                session.status = number_in_session > 1 ? 110 : 101;
                                printf("%d\n", session.status);
                            }
                            sessions[pi] = session;
                            continue;
                        }
                        /* if user, that make input, is current */ 
                        if (number_in_session == session.turn) {

                            if (c >= '1' && c <= '9') {
                                session.turn = session.status % 2 + 1;

                                int nc = c - '1';
                                int line = nc / 3;
                                int column = nc % 3;
                                if (session.field_statement[line][column] == 0) {
                                    session.field_statement[line][column] = session.status % 2 != 0 ? CROSS : ZERO;

                                    sprintf(outbuf, "step %d\n", session.status + 1);
                                    write(session.fp, &outbuf, 8);
                                    write(session.sp, &outbuf, 8);
                                    draw(session, sessions);
                                    session.status++;                        
                                } else {
                                    sprintf(outbuf, "incorrect input\n");
                                    write(p, &outbuf, 17);
                                }
                                int winner = wincon(session);
                                
                                if (winner == 1) {
                                    sprintf(outbuf, "\n1st player wins\n");
                                    write(session.fp, &outbuf, 18);
                                    write(session.sp, &outbuf, 18); 

                                    session.status = 101;
                                } else if (winner == 2) {
                                    sprintf(outbuf, "\n2nd player wins\n");
                                    write(session.fp, &outbuf, 18);
                                    write(session.sp, &outbuf, 19); 

                                    session.status = 110;
                                }

                            } else {
                                sprintf(outbuf, "incorrect input\n");
                                write(p, &outbuf, 16);
                            }
                        }
                        sessions[pi] = session;
                    }
                }
            }
        }   
    }
}

void close_server(int sfd, int * all_descs) {
    for (int i = 0; i < MAX_CONNECTIONS; i ++) { close(all_descs[i]); }
    close(sfd);
    unlink(PATH);
}

void create_session (int p, struct game_session session, struct game_session * sessions) {
    session.fp = p;
    session.status = -1;
    session.turn = 1;
    int start_fs[3][3] = {
        {NONE, NONE, NONE},
        {NONE, NONE, NONE},
        {NONE, NONE, NONE}
    };
    memcpy(session.field_statement, start_fs, sizeof(start_fs));
    char start_field[3][3] = {
        {'1', '2', '3'},
        {'4', '5', '6'},  
        {'7', '8', '9'}
    };
    memcpy(session.field, start_field, sizeof(start_field));
    for (int k = 0; k < MAX_CONNECTIONS / 2; k++) {
        if (sessions[k].status == NULL || sessions[k].status > 9 ) {
            sessions[k] = session;
            break;
        }
    }
}


int reg_cfd(int cfd, int * all_descs, int len) {
    int i = 0;
    for ( i = 0; i < len; ++ i ) {
        if ( all_descs[i] == 0 ) {
            all_descs[i] = cfd;
            
            return i;
        }
    }
    return -1;
}

int not_in(int elem, int * array) {
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (array[i] == elem) { return 0; }
    }
    return 1;
}

void draw(game_session session, game_session * sessions) {
    char buf[30] = {0};
    // matrix updating
    for (int line = 0; line < 3; line++) {
        for (int column = 0; column < 3; column++) {
            if (session.field_statement[line][column] == CROSS) {
                session.field[line][column] = 'x';
            } else  if (session.field_statement[line][column] == ZERO) {
                session.field[line][column] = '0';
            }
        }
    }
    int ctr = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j ++) {
            buf[ctr++] = session.field[i][j];
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
    write(session.fp, &buf, 30);
    write(session.sp, &buf, 30);    
}


int wincon(game_session session) {
    int target, line, column, eq, win = 0;

    for (int pl = 1; pl < 3; pl++) {
        target = pl == 1 ? CROSS : ZERO;

        // horizontal
        for (line = 0; line < 3; line++) {
            eq = 0;
            for (column = 0; column < 3; column++) {
                if (session.field[line][column] == target) {
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
                if (session.field[line][column] == target) {
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
            if (session.field[line][column] == target) {
                ++eq;
            }
            if (eq == 3) {
                win = 1;
            }
        }
        // diagonal L
        eq = 0;
        for (line = 0, column = 2; column >= 0 && line < 3; column--, line++) {
            if (session.field[line][column] == target) {
                ++eq;
            }
            if (eq == 3) {
                win = 1;
            }
        }
        if (win) { return pl; }
    }
}