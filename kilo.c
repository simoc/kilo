
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

struct termios orig_termios;

void
die(const char *s)
{
	perror(s);
	exit(1);
}

void
disableRawMode(void)
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
	{
		die("tcsetattr");
	}
}

void
enableRawMode(void)
{
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
	{
		die("tcgetattr");
	}
	atexit(disableRawMode);
	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT);
	raw.c_iflag &= ~(ICRNL);
	raw.c_iflag &= ~(INPCK);
	raw.c_iflag &= ~(ISTRIP);
	raw.c_iflag &= ~(IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_iflag &= ~(IXON);
	raw.c_lflag &= ~(ECHO);
	raw.c_lflag &= ~(ICANON);
	raw.c_lflag &= ~(IEXTEN);
	raw.c_lflag &= ~(ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
	{
		die("tcsetattr");
	}
}

int
main(int argc, char *argv[])
{
	enableRawMode();

	while (1)
	{
		char c = '\0';
		if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
		{
			die("read");
		}
		if (iscntrl(c))
		{
			printf("%d\r\n", c);
		}
		else
		{
			printf("%d ('%c')\r\n", c, c);
		}
		if (c == 'q')
		{
			break;
		}
	}
	return 0;
}
