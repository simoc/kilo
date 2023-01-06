#include "kilo_win32.h"

int
getline(char **line, size_t *linecap, FILE *fp)
{
	if (*line == NULL)
	{
		*linecap = 0;
	}

	int len = 0;
	int c = fgetc(fp);
	if (c == EOF)
	{
		return -1;
	}

	while (c != EOF)
	{
		if (len + 1 >= *linecap)
		{
			*linecap += 256;
			*line = realloc(*line, *linecap);
			if (*line == NULL)
			{
				return -1;
			}
			memset((*line) + len, '\0', (*linecap) - len);
		}
		(*line)[len] = c;
		len++;
		if (c == '\n')
		{
			break;
		}

		int next_c = fgetc(fp);
		if (c == '\r' && next_c == '\n')
		{
			(*line)[len] = next_c;
			len++;
			break;
		}
		c = next_c;
	}
	return len;
}
