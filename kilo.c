/*** includes ***/
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*** defines ***/
#define KILO_VERSION "0.0.1"

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
getCurrentPosition(int *rows, int *cols)
{
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
	{
		return -1;
	}

	/*
	 * Parse "\x1b[24;80R" rows and columns output.
	 */
	while (i < sizeof(buf) - 1)
	{
		if (read(STDIN_FILENO, &buf[i], 1) != 1)
		{
			break;
		}
		if (buf[i] == 'R')
		{
			break;
		}
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b')
	{
		return -1;
	}
	if (buf[1] != '[')
	{
		return -1;
	}
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
	{
		return -1;
	}

	return 0;
}

int
getWindowSize(int *rows, int *cols)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
		{
			return -1;
		}
		return getCurrentPosition(rows, cols);
	}
	else
	{
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** append buffer ***/

struct abuf
{
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void
abAppend(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL)
	{
		return;
	}
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void
abFree(struct abuf *ab)
{
	free(ab->b);
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
editorDrawRows(struct abuf *ab)
{
	int y;

	for (y = 0; y < E.screenrows; y++)
	{
		if (y == E.screenrows / 3)
		{
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome),
				"Kilo editor -- version %s", KILO_VERSION);
			if (welcomelen > E.screencols)
			{
				welcomelen = E.screencols;
			}
			abAppend(ab, welcome, welcomelen);
		}
		else
		{
			abAppend(ab, "~", 1);
		}

		abAppend(ab, "\x1b[K", 3);
		if (y < E.screenrows - 1)
		{
			abAppend(ab, "\r\n", 2);
		}
	}
}

void
editorRefreshScreen(void)
{
	struct abuf ab = ABUF_INIT;

	/* hide cursor */
	abAppend(&ab, "\x1b[?25l", 6);
	/* move cursor to top left */
	abAppend(&ab, "\x1b[H", 3);
	editorDrawRows(&ab);
	abAppend(&ab, "\x1b[H", 3);
	/* show cursor */
	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
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
