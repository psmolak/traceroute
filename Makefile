PROGRAM := traceroute
ARCHIVE := pawel_smolak.tar.bz2
FILES := main.c checksum.c checksum.h Makefile
CFLAGS := -std=c99 -pedantic -Wall -Wextra -Werror


$(PROGRAM): main.o checksum.o
	$(CC) $(CFLAGS) -o $@ $^

main.c: checksum.h
checksum.c: checksum.h

pack: $(ARCHIVE)

%.tar.bz2: $(FILES)
	mkdir $*
	cp $(FILES) $*
	tar cvf $*.tar $*
	bzip2 --force $*.tar
	rm -rf $*

clean:
	rm -rf *.o

distclean: clean
	rm -rf $(PROGRAM) $(ARCHIVE)

.PHONY: clean distclean pack
