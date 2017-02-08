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

static int mldev_getattr(const char *path, struct stat *stbuf)
{
  fprintf (stderr, "getattr: %s\n", path);

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (strcmp(path, "/foobar") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 1;
  } else
    return -ENOENT;

  return 0;
}

static int mldev_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
  char dirs[204][7];
  int i, n;

  fprintf (stderr, "readdir: %s\n", path);
  if (strcmp(path, "/") != 0)
    return -ENOENT;

  filler (buf, ".", NULL, 0);
  filler (buf, "..", NULL, 0);

  n = read_mfd (fd, dirs);
  for (i = 0; i < n; i++)
    filler (buf, dirs[i], NULL, 0);

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
