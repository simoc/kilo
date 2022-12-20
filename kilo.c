
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
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void
enableRawMode(void)
{
	tcgetattr(STDIN_FILENO, &orig_termios);
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
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int
main(int argc, char *argv[])
{
	enableRawMode();

	while (1)
	{
		char c = '\0';
		read(STDIN_FILENO, &c, 1);
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
