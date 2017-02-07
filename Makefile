CPPFLAGS=-D_FILE_OFFSET_BITS=64

all: mldev mlslv mount-mldev

mldev: src/mldev.h src/mldev.o
	$(CC) -o $@ $^

mlslv: src/mldev.h src/mlslv.o

mount-mldev: src/mldev.h src/mldev.o src/fuse.o
	$(CC) -o $@ $^ -lfuse

clean:
	-rm -f src/*.o
