CC = gcc
OFILE = img2ascii.out
IFILE = img2ascii.c
LIBS = -ljpeg -lpng -lm

$(OFILE) : $(IFILE)
	$(CC) -g -o $(OFILE) $(IFILE) $(LIBS)

clean :
	rm $(OFILE)
