# to open devenv in vim:
# tabe test.cpp | tabe tbuf.h | tabe fio.h | tabe in.txt

# to build everything
all:
	g++ --std=c++14 -g test.cpp -o test.out
clean:
	rm -f *.o test.out
