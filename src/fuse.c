#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "mldev.h"

static int fd;
static const char *current_path = "";
static off_t current_offset = 0;

static int mldev_close(const char *, struct fuse_file_info *);

static void *mldev_init(struct fuse_conn_info *conn)
{
  return NULL;
}

static void split_path (const char *path, char *sname, char *fn1, char *fn2)
{
  const char *start, *end;

  memset (sname, 0, 7);
  memset (fn1, 0, 7);
  memset (fn2, 0, 7);

  start = path + 1;
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
}

static int mldev_getattr(const char *path, struct stat *stbuf)
{
  char sname[7], fn1[7], fn2[7];

  split_path(path, sname, fn1, fn2);

  memset(stbuf, 0, sizeof(struct stat));

  if (sname[0] == 0)
    {
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 2;
    }
  else if (fn1[0] == 0)
    {
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 2;
    }
  else
    {
      stbuf->st_mode = S_IFREG | 0555;
      stbuf->st_nlink = 1;
    }

  return 0;
}

static int mldev_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
  char sname[7], fn1[7], fn2[7];
  char dirs[DIR_MAX][7];
  char files[DIR_MAX][15];
  int i, n;

  split_path(path, sname, fn1, fn2);

  filler (buf, ".", NULL, 0);
  filler (buf, "..", NULL, 0);

  if (sname[0] == 0)
    {
      n = read_mfd (fd, dirs);
      for (i = 0; i < n; i++)
	filler (buf, dirs[i], NULL, 0);
    }
  else
    {
      n = read_dir (fd, sname, files);
      for (i = 0; i < n; i++)
	{
	  struct stat st;
	  memset (&st, 0, sizeof st);
	  switch (files[i][0])
	    {
	    case '0': st.st_mode = S_IFREG; break;
	    case 'L': st.st_mode = S_IFLNK; break;
	    default: exit(1);
	    }
	  st.st_mode |= 0555;
	  st.st_nlink = 1;
	  filler (buf, files[i] + 1, &st, 0);
	}
    }
  return 0;
}

static int mldev_open(const char *path, struct fuse_file_info *fi)
{
  char sname[7], fn1[7], fn2[7];

  if (strcmp (path, current_path) != 0 && *current_path != 0)
    mldev_close (path, fi);

  split_path(path, sname, fn1, fn2);
  fprintf (stderr, "open: %s -> %s; %s %s\n", path, sname, fn1, fn2);

  if ((fi->flags & 3) != O_RDONLY)
    return -EACCES;

  /* In the default case, FUSE looks at st_size to see the size of
     the file.  With MLDEV, this takes some effort.  To avoid that,
     we can set the direct_io flag. */
  fi->direct_io = 1;

  fi->nonseekable = 1; /* Not sure about this. */

  open_file (fd, "DSK", fn1, fn2, sname);

  current_path = path;
  current_offset = 0;

  return 0;
}

static int mldev_close(const char *path, struct fuse_file_info *fi)
{
  char sname[7], fn1[7], fn2[7];

  split_path(path, sname, fn1, fn2);
  fprintf (stderr, "close: %s\n", path);

  close_file (fd);

  current_path = "";
  current_offset = 0;

  return 0;
}

/* This seems to be a good size for file read requests.  Larger sizes
   seem to confuse MLSLV to trigger RDATA replies to be bigger than
   the requested by CALLOC.  */
#define MAX_READ 0400

static int mldev_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
  char sname[7], fn1[7], fn2[7];
  word_t buffer[MAX_READ + 2];
  int n;

  split_path(path, sname, fn1, fn2);
  fprintf (stderr, "read: %s, size=%ld, offset=%ld\n", path, size, offset);

  if (offset != current_offset)
    {
      fprintf (stderr, "read: offset %ld != current %ld\n",
	       offset, current_offset);
      return -ESPIPE;
    }

  n = size / 5;
  if (n > MAX_READ)
    n = MAX_READ;
  if (n == 0)
    n = 1;
  n = read_file (fd, buffer, n);
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

static struct fuse_operations mldev =
{
  .init         = mldev_init,
  .getattr	= mldev_getattr,
  .readdir	= mldev_readdir,
  .open		= mldev_open,
  .release	= mldev_close,
  .read		= mldev_read,
  .readlink	= mldev_readlink,
};

int main (int argc, char **argv)
{
  fd = init ("192.168.1.100");

  return fuse_main(argc, argv, &mldev, NULL);
}
