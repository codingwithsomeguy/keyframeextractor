CC = gcc
AVLIBS = -lavcodec -lavutil -lavformat

all:	keyframeextractor
	@echo "done"

clean::
	rm -f keyframeextractor *.core *.o

keyframeextractor:	keyframeextractor.c
	$(CC) -o $@ keyframeextractor.c $(AVLIBS)
