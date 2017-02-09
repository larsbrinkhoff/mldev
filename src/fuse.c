#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "mldev.h"

int fd;

static void *mldev_init(struct fuse_conn_info *conn)
{
  return NULL;
}

static void split_path (const char *path, char *sname, char *fn1, char *fn2)
{
  const char *start, *end;

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
  fprintf (stderr, "getattr: %s -> %s %s %s\n", path, sname, fn1, fn2);

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
  char dirs[204][7];
  char files[204][14];
  int i, n;

  split_path(path, sname, fn1, fn2);

  fprintf (stderr, "readdir: %s -> %s %s %s\n", path, sname, fn1, fn2);

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
	filler (buf, files[i], NULL, 0);
    }
  return 0;
}

static int mldev_open(const char *path, struct fuse_file_info *fi)
{
  fprintf (stderr, "open: %s\n", path);

  if (strcmp(path, "/foobar") != 0)
    return -ENOENT;

  if ((fi->flags & 3) != O_RDONLY)
    return -EACCES;

  return 0;
}

static int mldev_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
  fprintf (stderr, "read: %s\n", path);
  return 0;
}

static struct fuse_operations mldev =
{
  .init         = mldev_init,
  .getattr	= mldev_getattr,
  .readdir	= mldev_readdir,
  .open		= mldev_open,
  .read		= mldev_read,
};

int main (int argc, char **argv)
{
  fd = init ("192.168.1.100");

  return fuse_main(argc, argv, &mldev, NULL);
}
