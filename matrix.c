#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>


#define CSI                     "\x1b["
#define ANSI_ClearScreen        CSI "2J"
#define ANSI_GotoYX             CSI "%d;%dH"

#define ANSI_DEFAULT            CSI "0m"
#define ANSI_BLACK              CSI "0;30m"
#define ANSI_RED                CSI "0;31m"
#define ANSI_GREEN              CSI "0;32m"
#define ANSI_YELLOW             CSI "0;33m"
#define ANSI_BLUE               CSI "0;34m"
#define ANSI_MAGENTA            CSI "0;35m"
#define ANSI_CYAN               CSI "0;36m"
#define ANSI_WHITE              CSI "0;37m"

#define MAX_ITEMS               30
#define MAX_SLOW                15
#define MAX_WORDLEN             80
#define MAX_SAMPLES             80


typedef struct
{
        int     x;
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

struct winsize Window;
wchar_t samples[MAX_SAMPLES][MAX_WORDLEN + 1];
int nSampleCount;
int ChangedWindow;

static struct termios initial_settings, new_settings;
static int peek_character = -1;

void init_keyboard()
{
        tcgetattr (0, &initial_settings);
        new_settings = initial_settings;
        new_settings.c_lflag &= ~ICANON;
        new_settings.c_lflag &= ~ECHO;
        new_settings.c_cc[VMIN] = 1;
        new_settings.c_cc[VTIME] = 0;
        tcsetattr (0, TCSANOW, &new_settings);
}

void close_keyboard()
{
        tcsetattr (0, TCSANOW, &initial_settings);
}

int _kbhit()
{
        unsigned char ch;
        int nread;

        if (peek_character != -1)
                return 1;

        new_settings.c_cc[VMIN] = 0;
        tcsetattr (0, TCSANOW, &new_settings);
        nread = read (0, &ch, 1);
        new_settings.c_cc[VMIN] = 1;
        tcsetattr (0, TCSANOW, &new_settings);

        if (nread == 1)
        {
                peek_character = ch;
                return 1;
        }

        return 0;
}

int _getch ()
{
        char ch;

        if (peek_character != -1)
        {
                ch = peek_character;
                peek_character = -1;
                return ch;
        }

        read (0, &ch, 1);
        return ch;
}

// Terminal change signal
void sigWinChanged (int nSignal)
{
        ChangedWindow = 1;
}

// get current terminal size
int GetWindowSize (void)
{
        return ioctl (STDIN_FILENO, TIOCGWINSZ, &Window);
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
                if (y > 0 && y <= Window.ws_row && pWord->pos.x <= Window.ws_col)
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
        static struct sigaction act;

        act.sa_handler = sigWinChanged;
        sigaction (SIGWINCH, &act, NULL);

        setlocale (LC_CTYPE, getenv ("LANG"));
        memset (items, 0x0, sizeof (items));
        srandom (0);

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

        ChangedWindow = 1;
        init_keyboard();
        while (1)
        {
                // check window
                if (ChangedWindow == 1)
                {
                        ChangedWindow = 0;
                        GetWindowSize();
                        fputs (ANSI_ClearScreen, stdout);
                }

                // Create new items
                make = 0;
                for (i = 0; i < MAX_ITEMS; i++)
                {
                        if (items[i].use != 'Y')
                        {
                                if (random() % 5 == 0)
                                {
                                        x = random () % (Window.ws_col - 1);
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

                if (_kbhit())   // any key to quit
                        break;
        }
        fputs (ANSI_DEFAULT, stdout);
        close_keyboard();

        return 0;
}
