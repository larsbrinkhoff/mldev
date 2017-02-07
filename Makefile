all: mldev mlslv

mldev: src/mldev.o
	$(CC) -o $@ $^

mlslv: src/mlslv.o

clean:
	-rm -f src/*.o
