#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
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

static char *trim (char *name)
{
  char *p = name + 5;
  while ((*p == ' ' || *p == 0) && p >= name)
    {
      *p = 0;
      p--;
    }
  for (p = name; *p; ++p)
    {
      *p = toupper (*p);
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

int read_mfd (int fd, struct mldev_file *files)
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
      files[i].pack = MLDEV_DIR;
      strncpy (files[i].fn1, p + 1, 6);
      unslash (files[i].fn1);
      trim (files[i].fn1);
      i++;
      p = strchr (p, '\n');
      if (p == NULL)
	break;
      p++;
    }

  return i;
}

int read_dir (int fd, char *dev, char *sname, struct mldev_file *files)
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
      if (p[2] == 'L')
	files[i].pack = MLDEV_LINK;
      else
	files[i].pack = 0;
      strncpy (files[i].fn1, p + 6, 6);
      strncpy (files[i].fn2, p + 13, 6);
      unslash (files[i].fn1);
      unslash (files[i].fn2);
      trim (files[i].fn2);
      i++;
      p = q;
      if (p == NULL)
	break;
      p++;
    }

  return i;
}

/**********************************************************************/ 

static int fd;
static const char *current_path = "";
static int current_offset = 0;
static int current_flags = 0;

static void split_path (const char *path, char *device, char *sname,
			char *fn1, char *fn2)
{
  const char *start, *end;

  strcpy (device, "DSK   ");
  memset (sname, 0, 7);
  memset (fn1, 0, 7);
  memset (fn2, 0, 7);

  start = path + 1;
  end = strchr (start, ':');
  if (end)
    {
      strncpy (device, start, end - start);
      start = end + 1;
    }
  else
    {
      strcpy (device, "DSK");
    }

  end = strchr (start, '/');
  if (end)
    {
      strncpy (sname, start, end - start);
      start = end + 1;
      strncpy (fn1, start, 6);
      strcpy (fn2, start + 7);
    }
  else
    {
      strcpy (sname, start);
      *fn1 = 0;
      *fn2 = 0;
    }

  trim (device);
  trim (fn1);
  trim (fn2);
  trim (sname);
}

int mldev_getattr(const char *path, struct mldev_file *file)
{
  split_path (path, file->device, file->sname, file->fn1, file->fn2);

  if (file->sname[0] == 0 || file->fn1[0] == 0)
    file->pack = MLDEV_DIR;
  else
    file->pack = 0;

  return 0;
}

int mldev_readdir (const char *path, struct mldev_file *files)
{
  char device[7], sname[7], fn1[7], fn2[7];
  int i, n;

  split_path(path, device, sname, fn1, fn2);
  fprintf (stderr, "readdir: %s -> %s: %s; %s %s\n",
	   path, device, sname, fn1, fn2);

  if (sname[0] == 0)
    n = read_mfd (fd, files);
  else
    n = read_dir (fd, device, sname, files);
  if (n < 0)
    return -EIO;

  return n;
}

int mldev_open (const char *path, int flags)
{
  char device[7], sname[7], fn1[7], fn2[7];
  int mode;
  int n;

  if (strcmp (path, current_path) != 0 && *current_path != 0)
    mldev_close (path);

  split_path(path, device, sname, fn1, fn2);
  fprintf (stderr, "open: %s -> %s: %s; %s %s\n",
	   path, device, sname, fn1, fn2);

  switch (flags)
    {
    case O_RDONLY: mode = 0; break;
    case O_WRONLY: mode = 1; break;
    default:       return -EACCES;
    }

  n = protoc_open (fd, device, fn1, fn2, sname, mode);
  if (n < 0)
    {
      if (n == -ENSFL)
	return -ENOENT;
      else
	return -EIO;
    }
  current_path = path;
  current_offset = 0;
  current_flags = (flags & 3);

  return 0;
}

int mldev_close(const char *path)
{
  char device[7], sname[7], fn1[7], fn2[7];

  split_path(path, device, sname, fn1, fn2);
  fprintf (stderr, "close: %s\n", path);

  switch (current_flags)
    {
    case O_RDONLY: protoc_iclose(fd); break;
    case O_WRONLY: protoc_oclose(fd); break;
    default: return -EIO;
    }

  current_path = "";
  current_offset = 0;
  current_flags = 0;

  return 0;
}

#define MAX_WRITE 0200

int mldev_write(const char *path, const char *buf, int size, int offset)
{
  char device[7], sname[7], fn1[7], fn2[7];
  word_t buffer[MAX_WRITE + 1];
  int n;

  split_path(path, device, sname, fn1, fn2);
  fprintf (stderr, "write: %s, size=%d, offset=%d\n", path, size, offset);

  if (offset != current_offset)
    {
      fprintf (stderr, "write: offset %d != current %d\n",
	       offset, current_offset);
      return -ESPIPE;
    }

  if (size == 0)
    return 0;

  if (size > 5 * MAX_WRITE)
    size = 5 * MAX_WRITE;
  n = (size + 4) / 5;

  buffer[0] = size;
  ascii_to_words (buffer + 1, n, buf);
  n++;

  n = protoc_write (fd, buffer, n);
  if (n < 0)
    return -EIO;

  current_offset += size;
  return size;
}

/* This seems to be a good size for file read requests.  Larger sizes
   seem to confuse MLSLV to trigger RDATA replies to be bigger than
   the requested by CALLOC.  */
#define MAX_READ 0400

int mldev_read (const char *path, char *buf, int size, int offset)
{
  char device[7], sname[7], fn1[7], fn2[7];
  word_t buffer[MAX_READ + 2];
  int n;

  split_path(path, device, sname, fn1, fn2);
  fprintf (stderr, "read: %s, size=%d, offset=%d\n", path, size, offset);

  if (offset != current_offset)
    {
      fprintf (stderr, "read: offset %d != current %d\n",
	       offset, current_offset);
      return -ESPIPE;
    }

  n = size / 5;
  if (n > MAX_READ)
    n = MAX_READ;
  if (n == 0)
    n = 1;
  n = protoc_read (fd, buffer, n);
  if (n < 0)
    return 0;

  words_to_ascii (buffer + 1, n - 1, buf);
  n = buffer[0];
  if (n > size)
    n = size;

  current_offset += n;
  return n;
}

static int mldev_readlink (const char *path, char *data, size_t n)
{
  fprintf (stderr, "readlink: %s\n", path);
  strcpy (data, "foo/bar");
  return 7;
}

void mldev_init (const char *host)
{
  fd = protoc_init (host);
}
