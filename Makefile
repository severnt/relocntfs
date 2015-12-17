CC=gcc

%.o: %.c
		$(CC) -c -o $@ $< $(CFLAGS)

relocntfs: relocntfs.o
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f relocntfs *.o

