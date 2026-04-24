CXX := g++
CXXFLAGS := -g

.PHONY: all clean

all: Main Run TesteBuscasSequenciais

Main: Main.cpp ObjectiveFunction.cpp Operation.cpp Buscas.cpp ObjectiveFunction.h Operation.h Buscas.h
	$(CXX) $(CXXFLAGS) Main.cpp ObjectiveFunction.cpp Operation.cpp Buscas.cpp -o Main

Run: Run.cpp
	$(CXX) $(CXXFLAGS) Run.cpp -o Run

TesteBuscasSequenciais: TesteBuscasSequenciais.cpp ObjectiveFunction.cpp Operation.cpp Buscas.cpp ObjectiveFunction.h Operation.h Buscas.h
	$(CXX) $(CXXFLAGS) TesteBuscasSequenciais.cpp ObjectiveFunction.cpp Operation.cpp Buscas.cpp -o TesteBuscasSequenciais

clean:
	rm -f Main Run TesteBuscasSequenciais
