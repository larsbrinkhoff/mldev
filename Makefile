CPPFLAGS=-D_FILE_OFFSET_BITS=64

HEADERS=src/mldev.h src/protoc.h src/io.h
OBJS=src/mldev.o src/protoc.o src/io.o

all: mount.mldev

# Install in /sbin to make "mount" happy.
install:
	install mount.mldev /sbin

mlslv: src/mldev.h src/mlslv.o

mount.mldev: src/fuse.o $(HEADERS) $(OBJS)
	$(CC) -o $@ src/fuse.o $(OBJS) -lfuse

clean:
	-rm -f src/*.o
