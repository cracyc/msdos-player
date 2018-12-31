CXX=i586-mingw32msvc-g++
CXXFLAGS=-O2 -Wall -Wextra -DHAS_I286
LIBS=-lwinmm -lpsapi
OBJS=msdos.o
EXE=msdos.exe

$(EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(EXE) $(OBJS) $(LIBS)

msdos.o: msdos.cpp msdos.h
	$(CXX) $(CXXFLAGS) -c msdos.cpp

clean:
	-$(RM) $(OBJS) $(EXE)
