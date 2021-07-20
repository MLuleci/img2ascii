FLAGS = -ljpeg -lpng -lm -g -Wall
SRC = $(wildcard *.cc)

i2a : $(SRC)
	$(CXX) -o $@ $^ $(FLAGS)

.PHONY: clean
clean :
	rm -f i2a *.o
