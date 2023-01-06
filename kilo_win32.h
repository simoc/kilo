#pragma once

#include <stdio.h>
#include <Windows.h>

struct termios
{
	DWORD console_input_mode; /* from GetConsoleMode() */
	DWORD console_output_mode; /* from GetConsoleMode() */
};

struct winsize
{
	int ws_col;
	int ws_row;
};

typedef unsigned int ssize_t;

int ftruncate(int fd, int len);

int getline(char **line, size_t *linecap, FILE *fp);
