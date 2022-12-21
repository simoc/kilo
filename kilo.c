/*** includes ***/
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
struct editorConfig
{
	int screenrows;
	int screencols;
	struct termios orig_termios;
};
struct editorConfig E;

/*** terminal ***/
void
die(const char *s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);
	exit(1);
}

void
disableRawMode(void)
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
	{
		die("tcsetattr");
	}
}

void
enableRawMode(void)
{
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
	{
		die("tcgetattr");
	}
	atexit(disableRawMode);
	struct termios raw = E.orig_termios;
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

char
editorReadKey(void)
{
	int nread;
	char c = '\0';

	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 && errno != EAGAIN)
		{
			die("read");
		}
	}
	return c;
}

int
getWindowSize(int *rows, int *cols)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
		return -1;
	}
	else
	{
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** input ***/

void
editorProcessKeypress(void)
{
	char c = editorReadKey();

	switch (c)
	{
	case CTRL_KEY('q'):
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
		exit(0);
		break;
	}
}

/*** output ***/

void
editorDrawRows(void)
{
	int y;

	for (y = 0; y < E.screenrows; y++)
	{
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}

void
editorRefreshScreen(void)
{
	/* clear screen */
	write(STDOUT_FILENO, "\x1b[2J", 4);
	/* move cursor to top left */
	write(STDOUT_FILENO, "\x1b[H", 3);
	editorDrawRows();
	write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** init ***/
void
initEditor(void)
{
	if (getWindowSize(&E.screenrows, &E.screencols) == -1)
	{
		die("getWindowSize");
	}
}

int
main(int argc, char *argv[])
{
	enableRawMode();
	initEditor();

	while (1)
	{
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}
