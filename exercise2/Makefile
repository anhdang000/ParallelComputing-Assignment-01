#!/bin/bash
.PHONY: test performance clean performance-u

test:
	g++ -std=c++20 -pthread correctness.cpp my_barrier.cpp -o correctness
	./correctness

performance:
	g++ -std=c++20 -pthread performance.cpp my_barrier.cpp -o performance
	./performance

performance-u:
	g++ -std=c++20 -pthread performance.cpp my_barrier.cpp -o performance
	./performance

clean:
	rm main correctness performance
