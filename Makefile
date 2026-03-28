CXX=g++
CXXFLAGS=-g -std=c++20 -Wall -Werror
.PHONY: ../libpropfile/libpropfile.a
all: refinery 
refinery: main.o compile.o link.o ../libpropfile/libpropfile.a
	$(CXX) $(CXXFLAGS) $^ -o $@
../libpropfile/libpropfile.a:
	make -C ../libpropfile
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<
clean:
	rm -f *.o refinery