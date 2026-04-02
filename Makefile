CXX := g++
CXXFLAGS := -g

.PHONY: all clean

all: Main Run

Main: Main.cpp ObjectiveFunction.cpp Operation.cpp ObjectiveFunction.h Operation.h Buscas.h
	$(CXX) $(CXXFLAGS) Main.cpp ObjectiveFunction.cpp Operation.cpp -o Main

Run: Run.cpp
	$(CXX) $(CXXFLAGS) Run.cpp -o Run

clean:
	rm -f Main Run
