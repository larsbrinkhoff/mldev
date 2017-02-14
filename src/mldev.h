#define MLDEV_DIR -1
#define MLDEV_LINK -2

#define DIR_MAX 204 /* Maximum number of files in a directory. */

struct mldev_file
{
  char device[7], sname[7];
  char fn1[7], fn2[7];
  int pack;
  int mdate, mtime, rdate;
};

extern void mldev_init (const char *);
extern int mldev_readdir (const char *, struct mldev_file *);
extern int mldev_getattr (const char *path, struct mldev_file *);
extern int mldev_open (const char *, int);
extern int mldev_close (const char *);
extern int mldev_read (const char *, char *, int, int);
extern int mldev_write (const char *, const char *, int, int);
