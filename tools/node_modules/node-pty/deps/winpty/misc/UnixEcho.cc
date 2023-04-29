/*
 * Unix test code that puts the terminal into raw mode, then echos typed
 * characters to stdout.  Derived from sample code in the Stevens book, posted
 * online at http://www.lafn.org/~dave/linux/terminalIO.html.
 */

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "FormatChar.h"

static struct termios   save_termios;
static int              term_saved;

/* RAW! mode */
int tty_raw(int fd)
{
    struct termios  buf;

    if (tcgetattr(fd, &save_termios) < 0) /* get the original state */
        return -1;

    buf = save_termios;

    /* echo off, canonical mode off, extended input
       processing off, signal chars off */
    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* no SIGINT on BREAK, CR-to-NL off, input parity
       check off, don't strip the 8th bit on input,
       ouput flow control off */
    buf.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);

    /* clear size bits, parity checking off */
    buf.c_cflag &= ~(CSIZE | PARENB);

    /* set 8 bits/char */
    buf.c_cflag |= CS8;

    /* output processing off */
    buf.c_oflag &= ~(OPOST);

    buf.c_cc[VMIN] = 1;  /* 1 byte at a time */
    buf.c_cc[VTIME] = 0; /* no timer on input */

    if (tcsetattr(fd, TCSAFLUSH, &buf) < 0)
        return -1;

    term_saved = 1;

    return 0;
}


/* set it to normal! */
int tty_reset(int fd)
{
    if (term_saved)
        if (tcsetattr(fd, TCSAFLUSH, &save_termios) < 0)
            return -1;

    return 0;
}


int main()
{
    tty_raw(0);

    int count = 0;
    while (true) {
        char ch;
        char buf[16];
        int actual = read(0, &ch, 1);
        if (actual != 1) {
            perror("read error");
            break;
        }
        formatChar(buf, ch);
        fputs(buf, stdout);
        fflush(stdout);
        if (ch == 3) // Ctrl-C
            break;
    }

    tty_reset(0);
    return 0;
}
