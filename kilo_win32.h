#pragma once

#include <stdio.h>
#include <Windows.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

#define TCSAFLUSH 0
#define BRKINT 0
#define ICRNL 0
#define INPCK 0
#define ISTRIP 0
#define IXON 0
#define OPOST 0
#define CS8 0
#define ECHO 0
#define ICANON 0
#define IEXTEN 0
#define ISIG 0
#define VMIN 0
#define VTIME 0

struct termios
{
	int c_cflag;
	int c_iflag;
	int c_lflag;
	int c_oflag;
	int c_cc[1];
};

struct winsize
{
	int ws_col;
	int ws_row;
};

typedef unsigned int ssize_t;

int ftruncate(int fd, int len);

int getline(char **line, size_t *linecap, FILE *fp);

int tcgetattr(int fd, struct termios *termios);

int tcsetattr(int fd, int flags, struct termios *termios);

#define TIOCGWINSZ 0
