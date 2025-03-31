#!/bin/bash
.PHONY: start start-u clean

start:
	g++ -std=c++20 -pthread main.cpp -o main
	./main

start-u:
	g++-10 -std=c++20 -pthread main.cpp -o main
	./main

clean:
	rm main 
