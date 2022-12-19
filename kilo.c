
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

struct termios orig_termios;

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
	raw.c_iflag &= ~(ICRNL);
	raw.c_oflag &= ~(OPOST);
	raw.c_iflag &= ~(IXON);
	raw.c_lflag &= ~(ECHO);
	raw.c_lflag &= ~(ICANON);
	raw.c_lflag &= ~(IEXTEN);
	raw.c_lflag &= ~(ISIG);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int
main(int argc, char *argv[])
{
	enableRawMode();

	char c;
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
	{
		if (iscntrl(c))
		{
			printf("%d\r\n", c);
		}
		else
		{
			printf("%d ('%c')\r\n", c, c);
		}
	}
	return 0;
}
