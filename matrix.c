#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>


#define CSI					"\x1b["
#define ANSI_ClearScreen	CSI "2J"
#define ANSI_GotoYX			CSI "%d;%dH"

#define ANSI_DEFAULT		CSI "0m"
#define ANSI_BLACK			CSI "0;30m"
#define ANSI_RED			CSI "0;31m"
#define ANSI_GREEN			CSI "0;32m"
#define ANSI_YELLOW			CSI "0;33m"
#define ANSI_BLUE			CSI "0;34m"
#define ANSI_MAGENTA		CSI "0;35m"
#define ANSI_CYAN			CSI "0;36m"
#define ANSI_WHITE			CSI "0;37m"

#define MAX_ITEMS			30
#define MAX_SLOW			15
#define MAX_WORDLEN			80
#define MAX_SAMPLES			80


typedef struct
{
	int	x;
	int y;
} Position;

typedef struct
{
	wchar_t text[MAX_WORDLEN + 1];
	Position pos;
	int term;
	int count;
	char use;
	const char* pcszColor;
} Word;


Position Window;
wchar_t samples[MAX_SAMPLES][MAX_WORDLEN + 1];
int nSampleCount;


// change stdin to nonblock mode.
void SetInputMode (void)
{
	fcntl (0, F_SETFL, fcntl (0, F_GETFL) | O_NONBLOCK);
}

// check enter key
int InputHasEnter (void)
{
	int i;
	char buffer [256];

	int nRead = read (0, buffer, sizeof (buffer));
	if (nRead > 0)
	{
		for (i = 0; i < nRead; i++)
			if (buffer[i] == '\n')
				return 1;
	}
	return 0;
}

// get current terminal size
int GetWindowSize (void)
{
	FILE *fp = popen ("stty size", "r");
	if (fp != NULL)
	{
		fscanf (fp, "%d %d", &Window.y, &Window.x);
		pclose (fp);
		return 1;
	}
	return 0;
}

// display item
int DisplayWord (const Word* pWord)
{
	int i, y, count = 0;
	const int len = wcslen (pWord->text);

	fputs (pWord->pcszColor, stdout);
	for (i = 0; i < len; i++)
	{
		y = pWord->pos.y - i;
		if (y > 0 && y <= Window.y && pWord->pos.x <= Window.x) 
		{
			fprintf (stdout, ANSI_GotoYX "%lc", y, pWord->pos.x, pWord->text[i]);
			count++;
		}
	}
	return count;
}

// read samples
int ReadSamples (const char* filename)
{
	int count = 0;
	int len;
	wchar_t *pRead = NULL, *pSave;

	FILE *fp = fopen (filename, "r");
	if (fp == NULL)
		return 0;

	do
	{
		pSave = samples[count];
		pRead = fgetws (pSave, MAX_WORDLEN-1, fp);
		if (pRead != NULL)
		{
			len = wcslen (pSave) - 1;
			if (pSave[len] == L'\n')
				pSave[len] = L' ';
			else if (pSave[len] != L' ')
				wcscpy (pSave + len, L" ");

			count++;
		}
	} while (pRead != NULL && count < MAX_SAMPLES);
	fclose (fp);

	return count;
}

// set new item
void SetItem (Word* pWord, int x)
{
	static const char* Colors[] = {
		ANSI_BLACK,
		ANSI_RED,
		ANSI_GREEN,
		ANSI_YELLOW,
		ANSI_BLUE,
		ANSI_MAGENTA,
		ANSI_CYAN,
		ANSI_WHITE
	};

	pWord->use = 'Y';
	pWord->pos.x = x;
	pWord->pos.y = 0;
	pWord->count = 0;
	pWord->term  = (random() % MAX_SLOW) + 1;
	wcscpy (pWord->text, samples[random() % nSampleCount]);
	pWord->pcszColor = Colors[random() % 8];
}

int main (int argc, char* argv[])
{
	int i, j, x, make, nearX;
	Word items [MAX_ITEMS];

	setlocale (LC_CTYPE, "en_US.utf8");
	memset (items, 0x0, sizeof (items));

	nSampleCount = 0;
	if (argc > 1)
		nSampleCount = ReadSamples (argv[1]);

	if (nSampleCount == 0)
	{
		wcscpy (samples[0], L"Hello world ");
		wcscpy (samples[1], L"Terminal matrix ");
		wcscpy (samples[2], L"Powered by Jang ");
		nSampleCount = 3;
	}

	GetWindowSize();
	SetInputMode();
	srandom (0);

	fputs (ANSI_ClearScreen, stdout);
	while (1)
	{
		// Create new items
		make = 0;
		for (i = 0; i < MAX_ITEMS; i++)
		{
			if (items[i].use != 'Y')
			{
				if (random() % 5 == 0)
				{
					x = random () % (Window.x - 1);
					x++;
					nearX = 0;

					for (j = 0; j < MAX_ITEMS; j++)
					{
						if (j == i)
							continue;
						if (items[j].use == 'Y')
						{
							if (x >= items[j].pos.x - 1 && x <= items[j].pos.x + 1)
							{
								nearX++;
								break;
							}
						}
					}

					if (nearX == 0)
					{
						SetItem (&items[i], x);
						make++;
						if (make >= 2)
							break;
					}
				}
			}
		}

		// display items
		for (i = 0; i < MAX_ITEMS; i++)
		{
			if (items[i].use == 'Y')
			{
				items[i].count++;
				if (items[i].count == items[i].term)
				{
					items[i].count = 0;
					items[i].pos.y++;
					if (DisplayWord (&items[i]) == 0)
						items[i].use = '\0';
				}
			}
		}

		fprintf (stdout, ANSI_GotoYX, 1, 1);
		fflush (stdout);
		usleep (50000);
		if (InputHasEnter())
			break;
	}
	fputs (ANSI_DEFAULT, stdout);

	return 0;
}

