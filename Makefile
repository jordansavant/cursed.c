# http://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/
CC=gcc
CFLAGS=-I.
BUILD = main.c dmg_gametime.c dm_draw.c
LIBS = -lncurses -lmenu
DEPS = dmg_gametime.h dm_draw.h dmg_world.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

make: $(BUILD) $(DEPS)
	gcc $(BUILD) $(LIBS) $(CFLAGS)

run: $(BUILD) $(DEPS)
	gcc $(BUILD) $(LIBS) $(CFLAGS) -o main.out && ./main.out

valgrind: $(BUILD) $(DEPS)
	gcc -g -O0 $(BUILD) $(LIBS) $(CFLAGS) -o main.out && /usr/bin/valgrind --leak-check=full --show-leak-kinds=all --suppressions=valgrind.suppression ./main.out

valgrind-full: $(BUILD) $(DEPS)
	gcc -g -O0 $(BUILD) $(LIBS) $(CFLAGS) -o main.out && /usr/bin/valgrind --leak-check=full --show-leak-kinds=all ./main.out
