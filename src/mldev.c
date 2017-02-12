#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "protoc.h"
#include "mldev.h"

static void sixbit_to_ascii (word_t word, char *ascii)
{
  int i;

  for (i = 0; i < 6; i++)
    {
      ascii[i] = 040 + ((word >> (6 * (5 - i))) & 077);
    }
  ascii[6] = 0;
}

static void word_to_ascii (word_t word, char *ascii)
{
  int i;
  for (i = 0; i < 5; i++)
    {
      *ascii++ = (word >> 29) & 0177;
      word <<= 7;
    }
  *ascii = 0;
}

void words_to_ascii (void *data, int n, char *ascii)
{
  word_t *word = data;
  int i;
  for (i = 0; i < n; i++)
    {
      word_to_ascii (*word++, ascii);
      ascii += 5;
    }
}

void ascii_to_words (void *data, int n, const char *ascii)
{
  word_t x, *word = data;
  int i, j;
  for (i = 0; i < n; i++)
    {
      x = 0;
      for (j = 0; j < 5; j++)
	{
	  x = (x << 7) + (*ascii++ & 0177);
	}
      *word++ = (x << 1);
    }
}

static char *unslash (char *name)
{
  char *p = name;
  while (*p)
    {
      if (*p == '/')
	*p = '|';
      p++;
    }
  return name;
}

static void print_date (FILE *f, word_t t)
{
  /* Bits 3.1-3.5 are the day, bits 3.6-3.9 are the month, and bits
     4.1-4.7 are the year. */

  int date = (t >> 18);
  int day = (date & 037);
  int month = (date & 0740);
  int year = (date & 0777000);

  fprintf (f, "%u-%02u-%02u", (year >> 9) + 1900, (month >> 5), day);

  if (year & 0600000)
    printf (" [WARNING: overflowed year field]");
}

static void print_datime (FILE *f, word_t t)
{
  /* The right half of this word is the time of day since midnight in
     half-seconds. */

  int seconds = (t & 0777777) / 2;
  int minutes = (seconds / 60);
  int hours = (minutes / 60);

  print_date (f, t);
  fprintf (f, " %02u:%02u:%02u", hours, (minutes % 60), (seconds % 60));
}

static void print_ascii (FILE *f, int n, word_t *words)
{
  char string[6];
  word_t word;
  int i;

  while (n--)
    {
      word = *words++;
      word_to_ascii (word, string);
      fputs (string, f);
    }
}

static int slurp_file (int fd, char *device, char *fn1, char *fn2,
		      char *sname, word_t *buffer, int size)
		       
{
  word_t args[5];
  word_t reply[11];
  int m, n;

  n = protoc_open (fd, device, fn1, fn2, sname, 0);
  if (n < 0)
    return -1;

  m = 0;
  for (;;)
    {
      n = protoc_read (fd, buffer, size);
      if (n < 0)
	break;
      memmove (buffer, buffer + 1, sizeof (word_t) * (n - 1));
      buffer += n - 1;
      size -= n - 1;
      m += n - 1;
    }

  protoc_iclose (fd);

  return m;
}

int read_mfd (int fd, char dirs[][7])
{
  int i, j, n;
  word_t buffer[1 + DIR_MAX * 2];
  char text[DIR_MAX * 9], *p;
  char filename[11];

  n = slurp_file (fd, "DSK", "M.F.D.", "(FILE)", "", buffer, DIR_MAX * 2);
  words_to_ascii (buffer, n, text);

  p = text;
  i = 0;
  while (*p != 0 && *p != '\f')
    {
      strncpy (dirs[i], p + 1, 6);
      dirs[i][6] = 0;
      unslash (dirs[i]);
      i++;
      p = strchr (p, '\n');
      if (p == NULL)
	break;
      p++;
    }

  return i;
}

int read_dir (int fd, char *dev, char *sname, char files[][15])
{
  char text[DIR_MAX * 50], *p, *q;
  word_t buffer[1 + DIR_MAX * 10];
  int i, n;

  n = slurp_file (fd, dev, ".FILE.", "(DIR)", sname, buffer, DIR_MAX * 10);
  if (n < 0)
    return -1;

  words_to_ascii (buffer + 1, n - 1, text);

  p = text;
  p = strchr (p, '\n') + 1; // Skip first line.
  p = strchr (p, '\n') + 1; // Skip second line too.

  i = 0;
  while (p != NULL && *p != 0 && *p != '\f')
    {
      q = strchr (p, '\n');
      if (q - p < 20)
	break;
      files[i][0] = p[2];
      strncpy (files[i] + 1, p + 6, 13);
      files[i][14] = 0;
      unslash (files[i]);
      i++;
      p = q;
      if (p == NULL)
	break;
      p++;
    }

  return i;
}
