all: mldev mlslv

mldev: src/mldev.h src/mldev.o
	$(CC) -o $@ $^

mlslv: src/mldev.h src/mlslv.o

clean:
	-rm -f src/*.o
