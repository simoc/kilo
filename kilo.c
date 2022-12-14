/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#ifdef _WIN32
#include "kilo_win32.h"
#else
#include <sys/ioctl.h>
#endif

#include <sys/types.h>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

/*** defines ***/
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

/*** data ***/

struct editorSyntax
{
	char *filetype;
	char **filematch;
	char **keywords;
	char *singleline_comment_start;
	char *multiline_comment_start;
	char *multiline_comment_end;
	int flags;
};

enum editorKey
{
	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_DOWN,
	ARROW_UP,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

enum editorHighlight
{
	HL_NORMAL = 0,
	HL_COMMENT,
	HL_MLCOMMENT,
	HL_KEYWORD1,
	HL_KEYWORD2,
	HL_STRING,
	HL_NUMBER,
	HL_MATCH
};

typedef struct erow
{
	/* row number in file, zero-based */
	int idx;
	int size;
	int rsize;
	char *chars;
	char *render;
	unsigned char *hl;
	/* is a multiline comment open? */
	int hl_open_comment;
} erow;

struct editorConfig
{
	/* current cursor position */
	int cx;
	int cy;
	/* index into erow.render */
	int rx;
	/* first row of file being displayed */
	int rowoff;
	/* first column of file being displayed */
	int coloff;
	/* size of screen */
	int screenrows;
	int screencols;
	int numrows;
	erow *row;
	/* is file changed since last modification? */
	int dirty;
	char *filename;
	char statusmsg[80];
	time_t statusmsg_time;
	/* file type for syntax highlighting */
	struct editorSyntax *syntax;
	struct termios orig_termios;
};
struct editorConfig E;

/*** filetypes ***/

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};

char *C_HL_keywords[] =
{
	"switch", "if", "while", "for", "break", "continue", "return", "else",
	"struct", "union", "typedef", "static", "enum", "class", "case",

	"int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
	"void|", NULL
};

struct editorSyntax HLDB[] =
{
	{
		"c",
		C_HL_extensions,
		C_HL_keywords,
		"//",
		"/*",
		"*/",
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen(void);
char *editorPrompt(char *prompt, void (*callback)(char *, int));

/*** terminal ***/
void
die(const char *s)
{
#ifdef _WIN32
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\x1b[2J", 4, NULL, NULL);
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\x1b[H", 3, NULL, NULL);
#else
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
#endif
	perror(s);
	exit(1);
}

void
disableRawMode(void)
{
#ifdef _WIN32
	if (SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),
		E.orig_termios.console_input_mode) == FALSE)
	{
		die("SetConsoleMode");
	}
	if (SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),
		E.orig_termios.console_output_mode) == FALSE)
	{
		die("SetConsoleMode");
	}
#else
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
	{
		die("tcsetattr");
	}
#endif
}

void
enableRawMode(void)
{
#ifdef _WIN32
	DWORD output_mode = 0;
	if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &output_mode) == FALSE)
	{
		die("GetConsoleMode");
	}
	E.orig_termios.console_output_mode = output_mode;

	DWORD input_mode = 0;
	if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &input_mode) == FALSE)
	{
		die("GetConsoleMode");
	}
	E.orig_termios.console_input_mode = input_mode;

	atexit(disableRawMode);

	output_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), output_mode) == FALSE)
	{
		die("SetConsoleMode");
	}

	input_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
	input_mode &= ~(ENABLE_LINE_INPUT);
	input_mode &= ~(ENABLE_ECHO_INPUT);
	if (SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), input_mode) == FALSE)
	{
		die("SetConsoleMode");
	}
#else
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
	{
		die("tcgetattr");
	}
	atexit(disableRawMode);
	struct termios raw = E.orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
	{
		die("tcsetattr");
	}
#endif
}

int
readKeypress(void)
{
	char c;
#ifdef _WIN32
	DWORD nread;
	if (ReadConsole(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &nread, NULL) == FALSE)
	{
		return -1;
	}
	if (nread == 0)
	{
		return -1;
	}
#else
	int nread;
	while ((nread = read(STDIN_FILENO, &c, 1)) == 0)
	{
	}
	if (nread < 0)
	{
		return -1;
	}
#endif

	return c;
}

int
editorReadKey(void)
{
	int c = 0;

	while (1)
	{
		c = readKeypress();
		if (c != -1)
		{
			break;
		}
#ifndef _WIN32
		if (errno != EAGAIN)
		{
			die("read");
		}
#endif
	}

	if (c == '\x1b')
	{
		char seq[3];
		int c0 = readKeypress();
		seq[0] = c0;

		if (c0 == -1)
		{
			return '\x1b';
		}

		int c1 = readKeypress();
		seq[1] = c1;
		if (c1 == -1)
		{
			return '\x1b';
		}

		if (seq[0] == '[')
		{
			if (seq[1] >= '0' && seq[1] <= '9')
			{
				int c2 = readKeypress();
				seq[2] = c2;
				if (c2 == -1)
				{
					return '\x1b';
				}

				if (seq[2] == '~')
				{
					switch (seq[1])
					{
					case '1':
						return HOME_KEY;
					case '3':
						return DEL_KEY;
					case '4':
						return END_KEY;
					case '5':
						return PAGE_UP;
					case '6':
						return PAGE_DOWN;
					case '7':
						return HOME_KEY;
					case '8':
						return END_KEY;
					}
				}
			}
			else
			{
				switch (seq[1])
				{
				case 'A':
					return ARROW_UP;
				case 'B':
					return ARROW_DOWN;
				case 'C':
					return ARROW_RIGHT;
				case 'D':
					return ARROW_LEFT;
				case 'H':
					return HOME_KEY;
				case 'F':
					return END_KEY;
				}
			}
		}
		else if (seq[0] == 'O')
		{
			switch (seq[1])
			{
			case 'H':
				return HOME_KEY;
			case 'F':
				return END_KEY;
			}
		}

		return '\x1b';
	}
	else
	{
		return c;
	}
}

#ifndef _WIN32
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
#endif

int
getWindowSize(int *rows, int *cols)
{
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) == FALSE)
	{
		return -1;
	}
	*cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	*rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	return 0;
#else
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
#endif
}

/*** syntax highlighting ***/

int
is_separator(int c)
{
	return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void
editorUpdateSyntax(erow *row)
{
	row->hl = realloc(row->hl, row->rsize);
	memset(row->hl, HL_NORMAL, row->rsize);

	if (E.syntax == NULL)
	{
		return;
	}
	char **keywords = E.syntax->keywords;

	char *scs = E.syntax->singleline_comment_start;
	char *mcs = E.syntax->multiline_comment_start;
	char *mce = E.syntax->multiline_comment_end;

	int scs_len = scs ? strlen(scs) : 0;
	int mcs_len = mcs ? strlen(mcs) : 0;
	int mce_len = mce ? strlen(mce) : 0;

	int prev_sep = 1;
	int in_string = 0;
	int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment != 0);

	int i = 0;
	while (i < row->rsize)
	{
		char c = row->render[i];
		unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

		if (scs_len > 0 && !in_string && !in_comment)
		{
			if (strncmp(&row->render[i], scs, scs_len) == 0)
			{
				/* highlight comment line */
				memset(&row->hl[i], HL_COMMENT, row->rsize - i);
				break;
			}
		}

		if (mcs_len > 0 && mce_len > 0 && !in_string)
		{
			if (in_comment)
			{
				row->hl[i] = HL_MLCOMMENT;
				if (strncmp(&row->render[i], mce, mce_len) == 0)
				{
					memset(&row->hl[i], HL_MLCOMMENT, mce_len);
					i += mce_len;
					in_comment = 0;
					prev_sep = 0;
					continue;
				}
				else
				{
					i++;
					continue;
				}
			}
			else if (strncmp(&row->render[i], mcs, mcs_len) == 0)
			{
				memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
				i += mcs_len;
				in_comment = 1;
				continue;
			}
		}

		if (E.syntax->flags & HL_HIGHLIGHT_STRINGS)
		{
			if (in_string)
			{
				row->hl[i] = HL_STRING;
				if (c == '\\' && i + 1 < row->rsize)
				{
					row->hl[i + 1] = HL_STRING;
					i += 2;
					continue;
				}
				if (c == in_string)
				{
					/* found matching closing quote */
					in_string = 0;
				}
				i++;
				prev_sep = 1;
				continue;
			}
			else
			{
				if (c == '"' || c == '\'')
				{
					in_string = c;
					row->hl[i] = HL_STRING;
					i++;
					continue;
				}
			}
		}

		if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS)
		{
			if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
				(c == '.' && prev_hl == HL_NUMBER))
			{
				row->hl[i] = HL_NUMBER;
				i++;
				prev_sep = 0;
				continue;
			}
		}

		if (prev_sep)
		{
			int j;
			for (j = 0; keywords[j] != NULL; j++)
			{
				int klen = strlen(keywords[j]);
				int kw2 = (keywords[j][klen -1] == '|');
				if (kw2 != 0)
				{
					klen--;
				}

				if (strncmp(&row->render[i], keywords[j], klen) == 0 &&
					is_separator(row->render[i + klen]))
				{
					memset(&row->hl[i], kw2 != 0 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
					i += klen;
					break;
				}
			}
			if (keywords[j] != NULL)
			{
				prev_sep = 0;
				continue;
			}
		}

		prev_sep = is_separator(c);
		i++;
	}

	int changed = (row->hl_open_comment != in_comment);
	row->hl_open_comment = in_comment;
	if (changed && row->idx + 1 < E.numrows)
	{
		editorUpdateSyntax(&E.row[row->idx + 1]);
	}
}

int
editorSyntaxToColour(int hl)
{
	switch (hl)
	{
	case HL_COMMENT:
	case HL_MLCOMMENT:
		return 36;
	case HL_KEYWORD1:
		return 33;
	case HL_KEYWORD2:
		return 32;
	case HL_STRING:
		return 35;
	case HL_NUMBER:
		return 31;
	case HL_MATCH:
		return 34;
	default:
		return 37;
	}
}

void
editorSelectSyntaxHighlight(void)
{
	E.syntax = NULL;
	if (E.filename == NULL)
	{
		return;
	}

	char *ext = strrchr(E.filename, '.');
	unsigned int j;
	for (j = 0; j < HLDB_ENTRIES; j++)
	{
		struct editorSyntax *s = &HLDB[j];
		unsigned int i = 0;
		while (s->filematch[i])
		{
			int is_ext = (s->filematch[i][0] == '.');
			if ((is_ext && ext && strcmp(ext, s->filematch[i]) == 0) ||
				(!is_ext && strstr(E.filename, s->filematch[i])))
			{
				E.syntax = s;

				int filerow;
				for (filerow = 0; filerow < E.numrows; filerow++)
				{
					editorUpdateSyntax(&E.row[filerow]);
				}
				return;
			}
			i++;
		}
	}
}

/*** row operations ***/

int
editorRowCxToRx(erow *row, int cx)
{
	int rx = 0;
	int j;

	for (j = 0; j < cx; j++)
	{
		if (row->chars[j] == '\t')
		{
			rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
		}
		rx++;
	}
	return rx;
}

int
editorRowRxToCx(erow *row, int rx)
{
	int cur_rx = 0;
	int cx;
	for (cx = 0; cx < row->size; cx++)
	{
		if (row->chars[cx] == '\t')
		{
			cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
		}
		cur_rx++;

		if (cur_rx > rx)
		{
			return cx;
		}
	}
	return cx;
}

void
editorUpdateRow(erow *row)
{
	int tabs = 0;
	int j;
	for (j = 0; j < row->size; j++)
	{
		if (row->chars[j] == '\t')
		{
			tabs++;
		}
	}

	free(row->render);
	row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);

	int idx = 0;
	for (j = 0; j < row->size; j++)
	{
		if (row->chars[j] == '\t')
		{
			row->render[idx++] = ' ';
			while (idx % KILO_TAB_STOP != 0)
			{
				row->render[idx++] = ' ';
			}
		}
		else
		{
			row->render[idx++] = row->chars[j];
		}
	}
	row->render[idx] = '\0';
	row->rsize = idx;

	editorUpdateSyntax(row);
}

void
editorInsertRow(int at, char *s, size_t len)
{
	if (at < 0 || at > E.numrows)
	{
		return;
	}

	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
	memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
	int j;
	for (j = at + 1; j <= E.numrows; j++)
	{
		E.row[j].idx++;
	}

	E.row[at].idx = at;

	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';

	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	E.row[at].hl = NULL;
	E.row[at].hl_open_comment = 0;
	editorUpdateRow(&E.row[at]);

	E.numrows++;
	E.dirty++;
}

void
editorFreeRow(erow *row)
{
	free(row->chars);
	free(row->render);
	free(row->hl);
}

void
editorDelRow(int at)
{
	if (at < 0 || at >= E.numrows)
	{
		return;
	}
	editorFreeRow(&E.row[at]);
	memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
	int j;
	for (j = at; j < E.numrows - 1; j++)
	{
		E.row[j].idx--;
	}
	E.numrows--;
	E.dirty++;
}

void
editorRowInsertChar(erow *row, int at, int c)
{
	if (at < 0 || at > row->size)
	{
		at = row->size;
	}
	row->chars = realloc(row->chars, row->size + 2);
	memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
	row->size++;
	row->chars[at] = c;
	editorUpdateRow(row);
	E.dirty++;
}

void
editorRowAppendString(erow *row, char *s, size_t len)
{
	row->chars = realloc(row->chars, row->size + len + 1);
	memcpy(&row->chars[row->size], s, len);
	row->size += len;
	row->chars[row->size] = '\0';
	editorUpdateRow(row);
	E.dirty++;
}

void
editorRowDelChar(erow *row, int at)
{
	if (at < 0 || at >= row->size)
	{
		return;
	}
	memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
	row->size--;
	editorUpdateRow(row);
	E.dirty++;
}

/*** editor operations ***/

void
editorInsertChar(int c)
{
	if (E.cy == E.numrows)
	{
		/* add empty line to end of file */
		editorInsertRow(E.numrows, "", 0);
	}
	editorRowInsertChar(&E.row[E.cy], E.cx, c);
	E.cx++;
}

void
editorInsertNewline(void)
{
	if (E.cx == 0)
	{
		editorInsertRow(E.cy, "", 0);
	}
	else
	{
		erow *row = &E.row[E.cy];
		editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
		row = &E.row[E.cy];
		row->size = E.cx;
		row->chars[row->size] = '\0';
		editorUpdateRow(row);
	}
	E.cy++;
	E.cx = 0;
}

void
editorDelChar(void)
{
	if (E.cy == E.numrows)
	{
		return;
	}
	if (E.cx == 0 && E.cy == 0)
	{
		return;
	}

	erow *row = &E.row[E.cy];
	if (E.cx > 0)
	{
		editorRowDelChar(row, E.cx - 1);
		E.cx--;
	}
	else
	{
		E.cx = E.row[E.cy - 1].size;
		editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
		editorDelRow(E.cy);
		E.cy--;
	}
}

/*** file i/o ***/

char *
editorRowsToString(size_t *buflen)
{
	int totlen = 0;
	int j;
	for (j = 0; j < E.numrows; j++)
	{
		totlen += E.row[j].size + 1;
	}
	*buflen = totlen;

	char *buf = malloc(totlen);
	char *p = buf;
	for (j = 0; j < E.numrows; j++)
	{
		memcpy(p, E.row[j].chars, E.row[j].size);
		p += E.row[j].size;
		*p = '\n';
		p++;
	}
	return buf;
}

void
editorOpen(char *filename)
{
	free(E.filename);
	E.filename = strdup(filename);

	editorSelectSyntaxHighlight();

	FILE *fp = fopen(filename, "r");
	if (!fp)
	{
		die("fopen");
	}

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp)) != -1)
	{
		while (linelen > 0 &&
			(line[linelen - 1] == '\r' || line[linelen - 1] == '\n'))
		{
			linelen--;
		}

		editorInsertRow(E.numrows, line, linelen);
	}
	free(line);
	fclose(fp);
	E.dirty = 0;
}

void
editorSave(void)
{
	int is_new_file = 0;

	if (E.filename == NULL)
	{
		E.filename = editorPrompt("Save as: %s (ESC or Ctrl-Q to cancel)", NULL);
		if (E.filename == NULL)
		{
			editorSetStatusMessage("Save aborted");
			return;
		}
		editorSelectSyntaxHighlight();
		is_new_file = 1;
	}

	size_t len;
	char *buf = editorRowsToString(&len);

	FILE *fp = fopen(E.filename, (is_new_file ? "wx" : "w"));
	if (fp)
	{
		if (fwrite(buf, 1, len, fp) == len)
		{
			fclose(fp);
			free(buf);
			E.dirty = 0;
			editorSetStatusMessage("%d bytes written to disk", len);
			return;
		}
		fclose(fp);
	}
	editorSetStatusMessage("Cannot save! I/O error: %s", strerror(errno));
	free(buf);

	if (is_new_file)
	{
		free(E.filename);
		E.filename = NULL;
	}
}

/*** find ***/

void
editorFindCallback(char *query, int key)
{
	static int last_match = -1;
	static int direction = 1;

	static int saved_hl_line = 0;
	static char *saved_hl = NULL;

	if (saved_hl)
	{
		memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
		free(saved_hl);
		saved_hl = NULL;
	}

	if (key == '\t' || key == '\x1b')
	{
		last_match = -1;
		direction = 1;
		return;
	}
	else if (key == ARROW_RIGHT || key == ARROW_DOWN)
	{
		direction = 1;
	}
	else if (key == ARROW_LEFT || key == ARROW_UP)
	{
		direction = -1;
	}
	else
	{
		last_match = -1;
		direction = 1;
	}

	if (last_match == -1)
	{
		direction = 1;
	}
	int current = last_match;

	int i;
	for (i = 0; i < E.numrows; i++)
	{
		current += direction;
		if (current < 0)
		{
			current = E.numrows - 1;
		}
		else if (current >= E.numrows)
		{
			current = 0;
		}
		erow *row = &E.row[current];
		char *match = strstr(row->render, query);
		if (match)
		{
			last_match = current;
			E.cy = current;
			E.cx = editorRowRxToCx(row, match - row->render);
			E.rowoff = E.numrows;

			saved_hl_line = current;
			saved_hl = malloc(row->rsize);
			memcpy(saved_hl, row->hl, row->rsize);
			memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
			break;
		}
	}
}

void
editorFind(void)
{
	int saved_cx = E.cx;
	int saved_cy = E.cy;
	int saved_coloff = E.coloff;
	int saved_rowoff = E.rowoff;

	char *query = editorPrompt("Search: %s (Use ESC/Ctrl-Q/Arrows/Enter)", editorFindCallback);
	if (query)
	{
		free(query);
	}
	else
	{
		E.cx = saved_cx;
		E.cy = saved_cy;
		E.coloff = saved_coloff;
		E.rowoff = saved_rowoff;
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

char *
editorPrompt(char *prompt, void (*callback)(char *, int))
{
	size_t bufsize = 128;
	char *buf = malloc(bufsize);

	size_t buflen = 0;
	buf[0] = '\0';

	while (1)
	{
		editorSetStatusMessage(prompt, buf);
		editorRefreshScreen();

		int c = editorReadKey();
		if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
		{
			if (buflen > 0)
			{
				buf[--buflen] = '\0';
			}
		}
		else if (c == '\x1b' || c == CTRL_KEY('q'))
		{
				editorSetStatusMessage("");
				if (callback)
				{
					callback(buf, c);
				}
				free(buf);
				return NULL;
		}
		else if (c == '\r')
		{
			if (buflen != 0)
			{
				editorSetStatusMessage("");
				if (callback)
				{
					callback(buf, c);
				}
				return buf;
			}
		}
		else if (!iscntrl(c) && c < 128)
		{
			if (buflen == bufsize - 1)
			{
				bufsize *= 2;
				buf = realloc(buf, bufsize);
			}
			buf[buflen++] = c;
			buf[buflen] = '\0';
		}

		if (callback)
		{
			callback(buf, c);
		}
	}
}

void
editorMoveCursor(int key)
{
	erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

	switch (key)
	{
	case ARROW_LEFT:
		if (E.cx > 0)
		{
			E.cx--;
		}
		else if (E.cy > 0)
		{
			E.cy--;
			E.cx = E.row[E.cy].size;
		}
		break;
	case ARROW_RIGHT:
		if (row && E.cx < row->size)
		{
			E.cx++;
		}
		else if (row && E.cx >= row->size)
		{
			E.cy++;
			E.cx = 0;
		}
		break;
	case ARROW_UP:
		if (E.cy > 0)
		{
			E.cy--;
		}
		break;
	case ARROW_DOWN:
		if (E.cy < E.numrows)
		{
			E.cy++;
		}
		break;
	}

	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = row ? E.row[E.cy].size : 0;
	if (E.cx > rowlen)
	{
		E.cx = rowlen;
	}
}

void
editorProcessKeypress(void)
{
	static int quit_times = KILO_QUIT_TIMES;

	int c = editorReadKey();

	switch (c)
	{
	case '\r':
		editorInsertNewline();
		break;

	case CTRL_KEY('q'):
		if (E.dirty > 0 && quit_times > 0)
		{
			editorSetStatusMessage("WARNING!!! File has unsaved changes."
				" Press Ctrl-Q %d more times to quit.", quit_times);
			quit_times--;
			return;
		}
#ifdef _WIN32
		WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\x1b[2J", 4, NULL, NULL);
		WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\x1b[H", 3, NULL, NULL);
#else
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
#endif
		exit(0);
		break;

	case CTRL_KEY('s'):
		editorSave();
		break;

	case HOME_KEY:
		E.cx = 0;
		break;

	case END_KEY:
		if (E.cy < E.numrows)
		{
			E.cx = E.row[E.cy].size;
		}
		break;

	case CTRL_KEY('f'):
		editorFind();
		break;

	case BACKSPACE:
	case CTRL_KEY('h'):
	case DEL_KEY:
		if (c == DEL_KEY)
		{
			editorMoveCursor(ARROW_RIGHT);
		}
		editorDelChar();
		break;

	case PAGE_UP:
	case PAGE_DOWN:
		{
			if (c == PAGE_UP)
			{
				E.cy = E.rowoff;
			}
			else if (c == PAGE_DOWN)
			{
				E.cy = E.rowoff + E.screenrows - 1;
				if (E.cy > E.numrows)
				{
					E.cy = E.numrows;
				}
			}
			int times = E.screenrows;
			while (times-- > 0)
			{
				editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
		}
		break;

	case ARROW_UP:
	case ARROW_DOWN:
	case ARROW_LEFT:
	case ARROW_RIGHT:
		editorMoveCursor(c);
		break;

	case CTRL_KEY('l'):
	case '\x1b':
		/* TODO */
		break;

	default:
		editorInsertChar(c);
		break;
	}

	quit_times = KILO_QUIT_TIMES;
}

/*** output ***/

void
editorScroll(void)
{
	E.rx = 0;
	if (E.cy <= E.numrows)
	{
		E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
	}

	if (E.cy < E.rowoff)
	{
		E.rowoff = E.cy;
	}
	if (E.cy >= E.rowoff + E.screenrows)
	{
		E.rowoff = E.cy - E.screenrows + 1;
	}
	if (E.rx < E.coloff)
	{
		E.coloff = E.rx;
	}
	if (E.rx >= E.coloff + E.screencols)
	{
		E.coloff = E.rx - E.screencols + 1;
	}
}

void
editorDrawRows(struct abuf *ab)
{
	int y;

	for (y = 0; y < E.screenrows; y++)
	{
		int filerow = y + E.rowoff;
		if (filerow >= E.numrows)
		{
			if (E.numrows == 0 && y == E.screenrows / 3)
			{
				char welcome[80];
				int welcomelen = snprintf(welcome, sizeof(welcome),
					"Kilo editor -- version %s", KILO_VERSION);
				if (welcomelen > E.screencols)
				{
					welcomelen = E.screencols;
				}
				int padding = (E.screencols - welcomelen) / 2;
				if (padding > 0)
				{
					abAppend(ab, "~", 1);
					padding--;
				}
				while (padding-- > 0)
				{
					abAppend(ab, " ", 1);
				}
				abAppend(ab, welcome, welcomelen);
			}
			else
			{
				abAppend(ab, "~", 1);
			}
		}
		else
		{
			int len = E.row[filerow].rsize - E.coloff;
			if (len < 0)
			{
				len = 0;
			}
			if (len > E.screencols)
			{
				len = E.screencols;
			}
			char *c = &E.row[filerow].render[E.coloff];
			unsigned char *hl = &E.row[filerow].hl[E.coloff];
			int current_colour = -1;
			int j;
			for (j = 0; j < len; j++)
			{
				if (iscntrl(c[j]))
				{
					char sym = (c[j] <= 26) ? '@' + c[j] : '?';
					abAppend(ab, "\x1b[7m", 4);
					abAppend(ab, &sym, 1);
					abAppend(ab, "\x1b[m", 3);
					if (current_colour != -1)
					{
						char buf[16];
						int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_colour);
						abAppend(ab, buf, clen);
					}
				}
				else if (hl[j] == HL_NORMAL)
				{
					if (current_colour != -1)
					{
						abAppend(ab, "\x1b[39m", 5);
						current_colour = -1;
					}
					abAppend(ab, &c[j], 1);
				}
				else
				{
					int colour = editorSyntaxToColour(hl[j]);
					if (colour != current_colour)
					{
						current_colour = colour;
						char buf[16];
						int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", colour);
						abAppend(ab, buf, clen);
					}
					abAppend(ab, &c[j], 1);
				}
			}
			abAppend(ab, "\x1b[39m", 5);
		}

		abAppend(ab, "\x1b[K", 3);
		abAppend(ab, "\r\n", 2);
	}
}

void
editorDrawStatusBar(struct abuf *ab)
{
	/* invert colours */
	abAppend(ab, "\x1b[7m", 4);
	char status[80];
	char rstatus[80];
	int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
		E.filename ? E.filename : "[No Name]",
		E.numrows,
		E.dirty != 0 ? "(modified)" : "");
	int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
		E.syntax ? E.syntax->filetype : "no ft",
		E.cy + 1, E.numrows);
	if (len > E.screencols)
	{
		len = E.screencols;
	}
	abAppend(ab, status, len);
	while (len < E.screencols)
	{
		if (E.screencols - len == rlen)
		{
			abAppend(ab, rstatus, rlen);
			break;
		}
		else
		{
			abAppend(ab, " ", 1);
		}
		len++;
	}
	/* restore normal colours */
	abAppend(ab, "\x1b[m", 3);
	abAppend(ab, "\r\n", 2);
}

void
editorDrawMessageBar(struct abuf *ab)
{
	/* clear line */
	abAppend(ab, "\x1b[K", 3);
	int msglen = strlen(E.statusmsg);
	if (msglen > E.screencols)
	{
		msglen = E.screencols;
	}
	if (msglen > 0 && time(NULL) - E.statusmsg_time < 5)
	{
		abAppend(ab, E.statusmsg, msglen);
	}
}

void
editorRefreshScreen(void)
{
	editorScroll();

	struct abuf ab = ABUF_INIT;

	/* hide cursor */
	abAppend(&ab, "\x1b[?25l", 6);
	/* move cursor to top left */
	abAppend(&ab, "\x1b[H", 3);
	editorDrawRows(&ab);
	editorDrawStatusBar(&ab);
	editorDrawMessageBar(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
		(E.cy - E.rowoff) + 1,
		(E.rx - E.coloff) + 1);
	abAppend(&ab, buf, strlen(buf));

	/* show cursor */
	abAppend(&ab, "\x1b[?25h", 6);

#ifdef _WIN32
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), ab.b, ab.len, NULL, NULL);
#else
	write(STDOUT_FILENO, ab.b, ab.len);
#endif
	abFree(&ab);
}

void
editorSetStatusMessage(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
	va_end(ap);
	E.statusmsg_time = time(NULL);
}

/*** init ***/
void
initEditor(void)
{
	E.cx = 0;
	E.cy = 0;
	E.rx = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.numrows = 0;
	E.row = NULL;
	E.dirty = 0;
	E.filename = NULL;
	E.statusmsg[0] = '\0';
	E.statusmsg_time = 0;
	E.syntax = NULL;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1)
	{
		die("getWindowSize");
	}

	/* make room for status bar and status message */
	E.screenrows -= 2;
}

int
main(int argc, char *argv[])
{
	enableRawMode();
	initEditor();
	if (argc >= 2)
	{
		editorOpen(argv[1]);
	}

	editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

	while (1)
	{
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}
