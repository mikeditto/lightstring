# First, install SDL2 and g++ using this command:
#   sudo apt-get install libsdl2-dev g++
# To compile:
#   make
# To run:
#   ./lightstring

LDLIBS = -lSDL2
CXXFLAGS = -O -I/usr/include/SDL2

all : lightstring

clean :
	$(RM) lightstring
