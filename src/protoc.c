#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "protoc.h"

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

static void flush_socket (int fd)
{
  int flag = 1;
  setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);
  flag = 0;
  setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);
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

static void send_words (int fd, word_t word1, word_t word2)
{
  unsigned char data[9];
  int i;

  data[0] = (word1 >> 28) & 0xff;
  data[1] = (word1 >> 20) & 0xff;
  data[2] = (word1 >> 12) & 0xff;
  data[3] = (word1 >>  4) & 0xff;
  data[4] = ((word1 <<  4) & 0xf0) + ((word2 >> 32) & 0x0f);
  data[5] = (word2 >> 24) & 0xff;
  data[6] = (word2 >> 16) & 0xff;
  data[7] = (word2 >>  8) & 0xff;
  data[8] = (word2 >>  0) & 0xff;

  ssize_t n = write (fd, data, 9);
  if (n != 9)
    {
      fprintf (stderr, "Write error: %ld\n", n);
      exit (1);
    }
}

static void read_octets (int fd, unsigned char *data)
{
  ssize_t n;
  int m;

  for (m = 0; m < 9; m += n)
    {
      n = read (fd, data + m, 9 - m);
      if (n == -1)
	{
	  fprintf (stderr, "Read error\n");
	  exit (1);
	}
    }
}

static void recv_words (int fd, word_t *word1, word_t *word2)
{
  unsigned char data[9];
  ssize_t n, m;
  word_t x;

  read_octets (fd, data);

  x = data[0];
  x = (x << 8) + data[1];
  x = (x << 8) + data[2];
  x = (x << 8) + data[3];
  *word1 = (x << 4) + (data[4] >> 4);

  x = data[4] & 0x0f;
  x = (x << 8) + data[5];
  x = (x << 8) + data[6];
  x = (x << 8) + data[7];
  *word2 = (x << 8) + data[8];
}

static int file_eof;
static int file_error;

static int request (int fd, int cmd, int n, word_t *args, word_t *reply)
{
  word_t aobjn;
  int i;

  file_eof = 0;
  file_error = 0;

  aobjn = (-n << 18) + cmd;
  send_words (fd, aobjn, args[0]);
  for (i = 1; i < n; i += 2)
    send_words (fd, args[i], args[i+1]);
  flush_socket (fd);

  if (reply == NULL)
    return 0;

 again:
  recv_words (fd, &aobjn, &reply[0]);
  n = (aobjn >> 18);
  if (n != 0)
    n = 01000000 - n;
  for (i = 1; i < n; i += 2)
    recv_words (fd, &reply[i], &reply[i+1]);

  switch (aobjn & 0777777LL)
    {
    case RDATA:
      fprintf (stderr, "RDATA: %012llo bytes\n", reply[0]);
      break;
    case ROPENI:
      if (reply[0] == 0777777777777LL)
	{
	  char device[7], fn1[7], fn2[7], sname[7];
	  fprintf (stderr, "ROPENI\n");
	  if (reply[5] != 0777777777777LL)
	    fprintf (stderr, "   %lld words, %lld %lld-bit bytes\n",
		     reply[7], reply[5], reply[6]);
	  if (reply[9] == 0777777777777LL)
	    {
	      //print_datime (stderr, reply[10]);
	      ;
	    }
	}
      else
	{
	  fprintf (stderr, "ROPENI: error %llo\n", reply[0]);
	  file_error = reply[0];
	}
      break;
    case ROPENO:
      if (reply[0] == 0777777777777LL)
	{
	  fprintf (stderr, "ROPENO\n");
	}
      else
	{
	  fprintf (stderr, "ROPENO: error %llo\n", reply[0]);
	  file_error = reply[0];
	}
      break;
    case REOF:
      fprintf (stderr, "REOF\n");
      file_eof = 1;
      if (cmd != CALLOC)
	goto again;
      break;
    case RNOOP:
      fprintf (stderr, "RNOOP: %012llo\n", reply[0]);
      break;
    case RICLOS:
      fprintf (stderr, "RICLOS\n");
      break;
    case ROCLOS:
      fprintf (stderr, "ROCLOS\n");
      break;
    case RIOC:
      fprintf (stderr, "RIOC\n");
      file_error = reply[0];
      break;
    default:
      fprintf (stderr, "Unknown reply: %012llo\n", aobjn);
      exit (1);
    }

  return n;
}

int protoc_open (int fd, char *device, char *fn1, char *fn2, char *sname,
		 int mode)
{
  word_t args[5];
  word_t reply[11];
  int cmd;
  int n;

  if ((mode & 1) == 0)
    cmd = COPENI;
  else
    cmd = COPENO;

  args[0] = ascii_to_sixbit (device);
  args[1] = ascii_to_sixbit (fn1);
  args[2] = ascii_to_sixbit (fn2);
  args[3] = ascii_to_sixbit (sname);
  args[4] = mode;
  fprintf (stderr, "%s: %s: %s; %s %s / %06o\n",
	   cmd == COPENO ? "COPENO" : "COPENI",
	   device, sname, fn1, fn2, mode);
  n = request (fd, cmd, 5, args, reply);
  if (file_error)
    return -file_error;

  return n;
}

int protoc_iclose (int fd)
{
  word_t args[1];
  word_t reply[1];

  args[0] = 0;
  fprintf (stderr, "CICLOS\n");
  return request (fd, CICLOS, 1, args, reply);
}

int protoc_oclose (int fd)
{
  word_t args[1];
  word_t reply[1];

  args[0] = 0;
  fprintf (stderr, "COCLOS\n");
  return request (fd, COCLOS, 1, args, reply);
}

int protoc_read (int fd, word_t *buffer, int size)
{
  word_t args[1];
  int n;

  args[0] = 5 * size;
  fprintf (stderr, "CALLOC: %012llo\n", args[0]);
  n = request (fd, CALLOC, 1, args, buffer);
  if (file_eof)
    return -1;
  else
    return n;
}

int protoc_write (int fd, word_t *args, int n)
{
  fprintf (stderr, "CDATA: n = %d, bytes = %012llo\n", n, args[0]);
  n = request (fd, CDATA, n, args, NULL);
  return 0;
}

int protoc_init (const char *host)
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
