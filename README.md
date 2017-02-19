# MLDEV

The "ML Device" protocol is a remote file system invented for the
PDP-10 MIT Incompatible Timesharing System.  It may have been the very
first transparent networking filesystem.

### Build

Install libfuse-dev, then type `make`.

### Usage

Create a mount point directory, and then mount an ITS file system like
this:

    sudo ./mount.mldev <hostname> <mountpoint>

I haven't bothered to figure out how FUSE works with user permissions,
so do everything as root.

Unmount like usual, with `umount <mountpoint>`.

FUSE accepts a number of command line options **in between** the host
name and mount point.  A useful one is `-f` which makes FUSE run in
the foreground and print debug messages.

### Bugs

- Doesn't work with binary files.  Only text files for now.
- Doesn't care about file metadata like file length, timestamps, or author.
- Doesn't work fully with `/` embedded in file names.
