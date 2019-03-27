PROGRAM := traceroute
ARCHIVE := pawel_smolak.tar.bz2
FILES := main.c misc.c misc.h checksum.c checksum.h Makefile
CFLAGS := -std=c99 -pedantic -Wall -Wextra -Werror


$(PROGRAM): main.o misc.o checksum.o
	$(CC) $(CFLAGS) -o $@ $^

main.c: config.h checksum.h misc.h

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
