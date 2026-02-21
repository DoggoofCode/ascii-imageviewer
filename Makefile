CC ?= cc
CFLAGS ?= -O0 -g -fsanitize=address
OBJ = main.o

imageviewer: $(OBJ)
	$(CC) $(CFLAGS) -o imageviewer $(OBJ)

.PHONY: clean 
clean:
	rm -f imageviewer *.o

