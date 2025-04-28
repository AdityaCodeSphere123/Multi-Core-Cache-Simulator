all:
	g++ main.cpp cache.cpp bus.cpp -o L1simulate

clean:
	rm -f L1simulate