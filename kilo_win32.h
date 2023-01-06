#pragma once

#include <stdio.h>
#include <Windows.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

struct termios
{
	DWORD console_mode;
};

struct winsize
{
	int ws_col;
	int ws_row;
};

typedef unsigned int ssize_t;

int ftruncate(int fd, int len);

int getline(char **line, size_t *linecap, FILE *fp);

#define TIOCGWINSZ 0
