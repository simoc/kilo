#include "kilo_win32.h"

int
ftruncate(int fd, int len)
{
	return 0;
}

int
getline(char **line, size_t *linecap, FILE *fp)
{
	return 0;
}

int
ioctl(int fd, int flags, struct winsize *ws)
{
	return 0;
}

int
tcgetattr(int fd, struct termios *termios)
{
	return 0;
}

int
tcsetattr(int fd, int flags, struct termios *termios)
{
	return 0;
}
