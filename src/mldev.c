#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#include "mldev.h"

#define DIR_MAX 204 /* Maximum number of files in a directory. */

static int client_socket (const char *host, int port)
{
  struct sockaddr_in address;
  int fd;

  memset (&address, '\0', sizeof address);
#if defined(__FreeBSD__) || defined(__OpenBSD__)
  address.sin_len = sizeof address;
#endif
  address.sin_family = PF_INET;
  address.sin_port = htons ((unsigned short)port);
  address.sin_addr.s_addr = inet_addr (host);

  if (address.sin_addr.s_addr == INADDR_NONE)
    {
      struct hostent *ent;

      ent = gethostbyname (host);
      if (ent == 0)
	return -1;

      memcpy (&address.sin_addr.s_addr, ent->h_addr, (size_t)ent->h_length);
    }

  fd = socket (PF_INET, SOCK_STREAM, 0);
  if (fd == -1)
    return -1;
  
  if (connect (fd, (struct sockaddr *)&address, sizeof address) == -1)
    {
      close (fd);
      return -1;
    } 

  return fd;
}

static void sixbit_to_ascii (word_t word, char *ascii)
{
  int i;

  for (i = 0; i < 6; i++)
    {
      ascii[i] = 040 + ((word >> (6 * (5 - i))) & 077);
    }
  ascii[6] = 0;
}

static word_t ascii_to_sixbit (char *ascii)
{
  char *spaces = "      ";
  word_t word = 0;
  int i;

  for (i = 0; i < 6; i++)
    {
      word <<= 6;
      if (*ascii == 0)
	ascii = spaces;
      word += ((*ascii++) - 040) & 077;
    }

  return word;
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

void words_to_ascii (word_t *word, int n, char *ascii)
{
  int i;
  for (i = 0; i < n; i++)
    {
      word_to_ascii (*word++, ascii);
      ascii += 5;
    }
  *ascii = 0;
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

static void send_word (int fd, word_t word)
{
  static int buffer = 0;
  unsigned char data;

  word &= 0777777777777LL;
  //fprintf (stderr, "send word %012llo\n", word);

  if (buffer < 0)
    {
      data = (~buffer << 4);
      data += (word >> 32) & 0x0f;
      write (fd, &data, 1);
      //fprintf (stderr, "sent byte: %ld (%02x)\n", write (fd, &data, 1), data);
      buffer = 0;
      word <<= 4;
    }
  else
    {
      buffer = ~(word & 0x0f);
    }

  int i;
  for (i = 0; i < 4; i++)
    {
      data = (word >> 28) & 0xff;
      write (fd, &data, 1);
      //fprintf (stderr, "sent byte: %ld (%02x)\n", write (fd, &data, 1), data);
      word <<= 8;
      word &= 0777777777777LL;
    }
}

static word_t recv_word (int fd)
{
  word_t word = 0;
  unsigned char data;
  static int buffer = 0;

  if (buffer < 0)
    {
      word = ~buffer;
    }

  int i;
  for (i = 0; i < 4; i++)
    {
      word <<= 8;
      read (fd, &data, 1);
      //fprintf (stderr, "recv byte: %ld (", read (fd, &data, 1));
      //fprintf (stderr, "%02x)\n", data);
      word += data;
    }

  if (buffer < 0)
    buffer = 0;
  else
    {
      read (fd, &data, 1);
      //fprintf (stderr, "recv byte: %ld (", read (fd, &data, 1));
      //fprintf (stderr, "%02x)\n", data);
      word <<= 4;
      word += (data >> 4) & 0x0f;
      buffer = ~(data & 0x0f);
    }

  //fprintf (stderr, "recv word %012llo\n", word);
  return word;
}

static int file_eof;
static int file_error;

static int request (int fd, int cmd, int n, word_t *args, word_t *reply)
{
  word_t aobjn;
  int i;

  file_error = 0;

  aobjn = (-n << 18) + cmd;
  send_word (fd, aobjn);
  for (i = 0; i < n; i++)
    send_word (fd, args[i]);

  aobjn = recv_word (fd);
  n = (aobjn >> 18);
  if (n != 0)
    n = 01000000 - n;
  for (i = 0; i < n; i++)
    reply[i] = recv_word (fd);
  if ((n & 1) == 0)
    recv_word (fd);

  switch (aobjn & 0777777LL)
    {
    case RDATA:
      fprintf (stderr, "RDATA: %012llo bytes\n", reply[0]);
      //for (i = 1; i < n; i++)
        //fprintf (stderr, "   %012llo\n", reply[i]);
      break;
    case ROPENI:
      if (reply[0] == 0777777777777LL)
	{
	  char device[7], fn1[7], fn2[7], sname[7];
	  sixbit_to_ascii (reply[1], device);
	  sixbit_to_ascii (reply[2], fn1);
	  sixbit_to_ascii (reply[3], fn2);
	  sixbit_to_ascii (reply[4], sname);
	  fprintf (stderr, "ROPENI: %s: %s; %s %s\n",
		   device, sname, fn1, fn2);
	  fprintf (stderr, "   %lld words, %lld %lld-bit bytes",
		   reply[7], reply[5], reply[6]);
	  if (reply[9] == 0777777777777LL)
	    {
	      fprintf (stderr, ", ");
	      print_datime (stderr, reply[10]);
	    }
	  fputc ('\n', stderr);
	}
      else
	{
	  fprintf (stderr, "ROPENI: error %llo\n", reply[0]);
	  file_error = reply[0];
	}
      break;
    case REOF:
      fprintf (stderr, "REOF\n");
      file_eof = 1;
      break;
    case RNOOP:
      fprintf (stderr, "RNOOP: %012llo\n", reply[0]);
      break;
    case RICLOS:
      fprintf (stderr, "RICLOS\n");
      break;
    case RIOC:
      fprintf (stderr, "RIOC\n");
      file_error = reply[0];
      break;
    }

  return n;
}

int open_file (int fd, char *device, char *fn1, char *fn2, char *sname)
{
  word_t args[5];
  word_t reply[11];
  int n;

  args[0] = ascii_to_sixbit (device);
  args[1] = ascii_to_sixbit (fn1);
  args[2] = ascii_to_sixbit (fn2);
  args[3] = ascii_to_sixbit (sname);
  args[4] = 0;
  n = request (fd, COPENI, 5, args, reply);

  return n;
}

int close_file (int fd)
{
  word_t args[1];
  word_t reply[1];

  args[0] = 0;
  return request (fd, CICLOS, 1, args, reply);
}

int read_file (int fd, word_t *buffer, int size)
{
  word_t args[1];
  int n;

  args[0] = 5 * size;
  n = request (fd, CALLOC, 1, args, buffer);
  return n;
}

static int slurp_file (int fd, char *device, char *fn1, char *fn2,
		      char *sname, word_t *buffer, int size)
		       
{
  word_t args[5];
  word_t reply[11];
  int m, n;

  open_file (fd, device, fn1, fn2, sname);
  if (file_error)
    return -1;

  m = 0;
  file_eof = 0;
  while (!file_eof)
    {
      n = read_file (fd, buffer, size);
      print_ascii (stderr, n - 1, buffer + 1);
      fputc ('\n', stderr);
      m += n - 1;
    }

  close_file (fd);

  return m;
}

int read_mfd (int fd, char dirs[204][7])
{
  int i, j, n, p;
  word_t buffer[0201];
  char filename[11];

  n = slurp_file (fd, "DSK", "M.F.D.", "(FILE)", "", buffer, 0200);

  p = 0;
  j = 0;
  for (i = 1; i < n; i++)
    {
      word_to_ascii (buffer[i], filename + p);
      p += 5;
      if (p >= 9)
	{
	  memcpy (dirs[j], filename + 1, 6);
	  dirs[j][6] = 0;
	  j++;

	  memmove (filename, filename + 9, p - 9);
	  p -= 9;
	}
    }

  return j;
}

int read_dir (int fd, char *sname, char files[204][14])
{
  char text[2048], *p;
  word_t buffer[0201];
  int i, n;

  n = slurp_file (fd, "DSK", ".FILE.", "(DIR)", sname, buffer, 0200);
  words_to_ascii (buffer + 1, n - 1, text);

  p = text;
  p = strchr (p, '\n') + 1; // Skip first line.
  p = strchr (p, '\n') + 1; // Skip second line too.

  i = 0;
  while (p != NULL && *p != 0 && *p != '\f')
    {
      strncpy (files[i], p + 6, 13);
      i++;
      p = strchr (p, '\n');
      if (p == NULL)
	break;
      p++;
    }

  return i;
}

int init (char *host)
{
  int fd;

  fd = client_socket (host, MLDEV_PORT);
  if (fd == -1)
    {
      fprintf (stderr, "Error opening connection.\n");
      exit (1);
    }

  return fd;
}

#pragma weak main

int main (int argc, char **argv)
{
  int fd, n;
  word_t args[11];
  word_t reply[11];
  char dirs[204][7];
  char files[204][14];

  fd = init ("192.168.1.100");

  args[0] = 42;
  n = request (fd, CNOOP, 1, args, reply);
  
  slurp_file (fd, "DSK", "LARS", "EMACS", "LARS", reply, 10);
  read_dir (fd, "LARS", files);
  read_mfd (fd, dirs);

  return 0;
}
