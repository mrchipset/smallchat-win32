/* smallchat-client.c -- Client program for smallchat-server.
 *
 * Copyright (c) 2023, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the project name of nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <io.h>

#include <winsock.h>
#include <Windows.h>

#include "chatlib.h"

/* ============================================================================
 * Low level terminal handling.
 * ========================================================================== */

void disableRawModeAtExit(void);

/* Raw mode: 1960 magic shit. */
int setRawMode(HANDLE fd, int enable) {
#if 0
    /* We have a bit of global state (but local in scope) here.
     * This is needed to correctly set/undo raw mode. */
    static struct termios orig_termios; // Save original terminal status here.
    static int atexit_registered = 0;   // Avoid registering atexit() many times.
    static int rawmode_is_set = 0;      // True if raw mode was enabled.

    struct termios raw;

    /* If enable is zero, we just have to disable raw mode if it is
     * currently set. */
    if (enable == 0) {
        /* Don't even check the return value as it's too late. */
        if (rawmode_is_set && tcsetattr(fd,TCSAFLUSH,&orig_termios) != -1)
            rawmode_is_set = 0;
        return 0;
    }

    /* Enable raw mode. */
    if (!isatty(fd)) goto fatal;
    if (!atexit_registered) {
        atexit(disableRawModeAtExit);
        atexit_registered = 1;
    }
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - do nothing. We want post processing enabled so that
     * \n will be automatically translated to \r\n. */
    // raw.c_oflag &= ...
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * but take signal chars (^Z,^C) enabled. */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    rawmode_is_set = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
#else
     /* We have a bit of global state (but local in scope) here.
     * This is needed to correctly set/undo raw mode. */
    static DWORD orig_mode;
    static int atexit_registered = 0;   // Avoid registering atexit() many times.
    static int rawmode_is_set = 0;      // True if raw mode was enabled.
    DWORD mode;

    /* If enable is zero, we just have to disable raw mode if it is
     * currently set. */
    if (enable == 0) {
        /* Don't even check the return value as it's too late. */
        if (rawmode_is_set && SetConsoleMode(fd, orig_mode) != 0)
            rawmode_is_set = 0;
        return 0;
    }

    /* Enable raw mode. */
    // if (!_isatty(fd)) goto fatal;
    if (!atexit_registered) {
        atexit(disableRawModeAtExit);
        atexit_registered = 1;
    }
    if (GetConsoleMode(fd,&orig_mode) == -1) return -1;

    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    mode = orig_mode;
    mode &= ~(ENABLE_MOUSE_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_WINDOW_INPUT);

    /* put terminal in raw mode */
    if (SetConsoleMode(fd, mode) == 0) return -1;
    FlushConsoleInputBuffer(fd);
    rawmode_is_set = 1;
    return 0;

// fatal:
//     errno = ENOTTY;
//     return -1;
//     GetConsoleMode(fd, &mode);
//     if(!SetConsoleMode(fd, 0)) {

//     }
#endif

}

/* At exit we'll try to fix the terminal to the initial conditions. */
void disableRawModeAtExit(void) {
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    setRawMode(hStdIn,0);
}

/* ============================================================================
 * Mininal line editing.
 * ========================================================================== */

void terminalCleanCurrentLine(void) {
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    // WriteFile(hStdOut, "\e[2K", 4, NULL, NULL);
    // write(fileno(stdout),"\e[2K",4);
}

void terminalCursorAtLineStart(void) {
    // write(fileno(stdout),"\r",1);
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteFile(hStdOut, "\r", 1, NULL, NULL);
}

#define IB_MAX 128
struct InputBuffer {
    char buf[IB_MAX];       // Buffer holding the data.
    int len;                // Current length.
};

/* inputBuffer*() return values: */
#define IB_ERR 0        // Sorry, unable to comply.
#define IB_OK 1         // Ok, got the new char, did the operation, ...
#define IB_GOTLINE 2    // Hey, now there is a well formed line to read.

/* Append the specified character to the buffer. */
int inputBufferAppend(struct InputBuffer *ib, int c) {
    if (ib->len >= IB_MAX) return IB_ERR; // No room.

    ib->buf[ib->len] = c;
    ib->len++;
    return IB_OK;
}

void inputBufferHide(struct InputBuffer *ib);
void inputBufferShow(struct InputBuffer *ib);

/* Process every new keystroke arriving from the keyboard. As a side effect
 * the input buffer state is modified in order to reflect the current line
 * the user is typing, so that reading the input buffer 'buf' for 'len'
 * bytes will contain it. */
int inputBufferFeedChar(struct InputBuffer *ib, int c) {
    switch(c) {
    case '\n':
        break;          // Ignored. We handle \r instead.
    case '\r':
        return IB_GOTLINE;
    case 127:           // Backspace.
        if (ib->len > 0) {
            ib->len--;
            inputBufferHide(ib);
            inputBufferShow(ib);
        }
        break;
    default:
        if (inputBufferAppend(ib,c) == IB_OK)
            write(fileno(stdout),ib->buf+ib->len-1,1);
        break;
    }
    return IB_OK;
}

/* Hide the line the user is typing. */
void inputBufferHide(struct InputBuffer *ib) {
    (void)ib; // Not used var, but is conceptually part of the API.
    terminalCleanCurrentLine();
    terminalCursorAtLineStart();
}

/* Show again the current line. Usually called after InputBufferHide(). */
void inputBufferShow(struct InputBuffer *ib) {
    write(fileno(stdout),ib->buf,ib->len);
}

/* Reset the buffer to be empty. */
void inputBufferClear(struct InputBuffer *ib) {
    ib->len = 0;
    inputBufferHide(ib);
}

/* =============================================================================
 * Main program logic, finally :)
 * ========================================================================== */

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <host> <port>\n", argv[0]);
        exit(1);
    }

    if(initWSA() != 0) {
        exit(1);
    }

    /* Create a TCP connection with the server. */
    int s = TCPConnect(argv[1],atoi(argv[2]),0);
    if (s == -1) {
        perror("Connecting to server");
        exit(1);
    }


    /* Put the terminal in raw mode: this way we will receive every
     * single key stroke as soon as the user types it. No buffering
     * nor translation of escape sequences of any kind. */
    setRawMode(GetStdHandle(STD_INPUT_HANDLE),1);

    fd_set readfds;
    struct InputBuffer ib;
    inputBufferClear(&ib);
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    while(1) {

        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        int maxfd = s;
        char buf[128]; /* Generic buffer for both code paths. */
        DWORD dwBuff = 128;
        DWORD dwRead = 0;
        DWORD res = WaitForSingleObject(hStdIn, 10);
        DWORD ret;
        switch (res)
        {
        case WAIT_OBJECT_0: // stdin
            /* code */
            ret = ReadConsole(hStdIn, buf, dwBuff, &dwRead, NULL);
            if (ret == 0) {
                printf("Error: %d\n", GetLastError());
            } else {
                for (int j = 0; j < dwRead; ++j) {
                    int res = inputBufferFeedChar(&ib, buf[j]);
                    switch (res)
                    {
                    case IB_GOTLINE:
                        inputBufferAppend(&ib,'\n');
                        inputBufferHide(&ib);
                        WriteFile(hStdOut,"you> ", 5, NULL, NULL);
                        WriteFile(hStdOut,ib.buf,ib.len, NULL, NULL);
                        send(s,ib.buf,ib.len, 0);
                        inputBufferClear(&ib);
                        break;
                    case IB_OK:
                        break;
                    }
                }

            }
            
            break;
        case WAIT_TIMEOUT:
            break;
        case WAIT_FAILED:
            printf("Error: %d", GetLastError());
            exit(1);
            break;
        default:
            continue;
        }
      
        
        int num_events = select(maxfd+1, &readfds, NULL, NULL, &timeout);
        if (num_events == -1) {
            perror("select() error");
            exit(1);
        } else if (num_events) {
            char buf[128]; /* Generic buffer for both code paths. */

            if (FD_ISSET(s, &readfds)) {
                /* Data from the server? */
                ssize_t count = recv(s,buf,sizeof(buf), 0);
                if (count <= 0) {
                    printf("Connection lost\n");
                    exit(1);
                }
                inputBufferHide(&ib);
                // send(fileno(stdout),buf,count, 0);
                WriteConsole(hStdOut, buf, count, NULL, NULL);
                inputBufferShow(&ib);
            }
            //  else if (FD_ISSET(stdin_fd, &readfds)) {
            //     /* Data from the user typing on the terminal? */
            //     ssize_t count = recv(stdin_fd,buf,sizeof(buf), 0);
            //     for (int j = 0; j < count; j++) {
            //         int res = inputBufferFeedChar(&ib,buf[j]);
            //         switch(res) {
            //         case IB_GOTLINE:
            //             inputBufferAppend(&ib,'\n');
            //             inputBufferHide(&ib);
            //             send(fileno(stdout),"you> ", 5, 0);
            //             send(fileno(stdout),ib.buf,ib.len, 0);
            //             send(s,ib.buf,ib.len, 0);
            //             inputBufferClear(&ib);
            //             break;
            //         case IB_OK:
            //             break;
            //         }
            //     }
            // }
        }
    }

    closesocket(s);
    cleanWSA();
    return 0;
}
