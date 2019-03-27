PROGRAM := traceroute
ARCHIVE := pawel_smolak.tar.bz2
FILES := main.c config.h utils.c utils.h Makefile
CFLAGS := -std=c99 -pedantic -Wall -Wextra -Werror


$(PROGRAM): main.o utils.o
	$(CC) $(CFLAGS) -o $@ $^

main.c: config.h utils.h

pack: $(ARCHIVE)

%.tar.bz2: $(FILES)
	mkdir $*
	cp $(FILES) $*
	tar cvf $*.tar $*
	bzip2 --force $*.tar
	rm -rf $*

format:
	clang-format -style=file -i *.c *.h

clean:
	rm -rf *.o

distclean: clean
	rm -rf $(PROGRAM) $(ARCHIVE)

.PHONY: clean distclean pack format
