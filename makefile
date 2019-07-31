CC = gcc
OFILE = img2ascii.out
IFILE = img2ascii.c
LIBS = -ljpeg

$(OFILE) : $(IFILE)
	$(CC) -o $(OFILE) $(IFILE) $(LIBS)

clean :
	rm $(OFILE)
