all: clean
	g++ -g -std=c++20 main.cpp decompile.cpp -o refinery -Wall -Werror
clean:
	rm refinery || true
