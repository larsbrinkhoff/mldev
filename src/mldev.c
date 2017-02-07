#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "mldev.h"

typedef long long word_t;

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

static void send_word (int fd, word_t word)
{
  static int buffer = 0;
  unsigned char data;

  word &= 0777777777777LL;
  fprintf (stderr, "send word %012llo\n", word);

  if (buffer < 0)
    {
      data = (~buffer << 4);
      data += (word >> 32) & 0x0f;
      fprintf (stderr, "sent byte: %ld (%02x)\n", write (fd, &data, 1), data);
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
      fprintf (stderr, "sent byte: %ld (%02x)\n", write (fd, &data, 1), data);
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
      fprintf (stderr, "recv byte: %ld (", read (fd, &data, 1));
      fprintf (stderr, "%02x)\n", data);
      word += data;
    }

  if (buffer < 0)
    buffer = 0;
  else
    {
      fprintf (stderr, "recv byte: %ld (", read (fd, &data, 1));
      fprintf (stderr, "%02x)\n", data);
      word <<= 4;
      word += (data >> 4) & 0x0f;
      buffer = ~(data & 0x0f);
    }

  fprintf (stderr, "recv word %012llo\n", word);
  return word;
}

static int request (int fd, int cmd, int n, word_t *args, word_t *reply)
{
  word_t aobjn;
  int i;

  aobjn = (-n << 18) + cmd;
  send_word (fd, aobjn);
  for (i = 0; i < n; i++)
    send_word (fd, args[i]);

  aobjn = recv_word (fd);
  n = 01000000 - (aobjn >> 18);
  for (i = 0; i < n; i++)
    reply[i] = recv_word (fd);

  return n;
}

int main (int argc, char **argv)
{
  int fd, n;
  word_t args[11];
  word_t reply[11];

  fd = client_socket ("192.168.1.100", MLDEV_PORT);
  printf ("fd = %d\n", fd);

  args[0] = 42;
  n = request (fd, CNOOP, 1, args, reply);

  return 0;
}
