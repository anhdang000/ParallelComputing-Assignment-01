
.PHONY: test clean benchmark performance
test:
	g++ -std=c++20 -pthread correctness.cpp -o correctness
	./correctness

benchmark:
	g++ -std=c++20 -pthread locks_benchmark.cpp -o benchmark
	./benchmark

performance:
	g++ -std=c++20 -pthread performance.cpp -o performance
	./performance

clean:
	rm correctness performance
