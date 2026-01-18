CXX      := g++
CXXFLAGS := -std=c++17 -O3 -march=native -funroll-loops -Wall -Wextra -pedantic -Iinclude -MMD -MP
LDFLAGS  :=

SRCDIR   := src
BINDIR   := bin
TARGET   := $(BINDIR)/orderbook

SRC      := $(wildcard $(SRCDIR)/*.cpp)
OBJ      := $(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%.o,$(SRC))
DEP      := $(OBJ:.o=.d)

.PHONY: build clean
build: $(TARGET)

$(TARGET): $(OBJ) | $(BINDIR)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ)

$(BINDIR)/%.o: $(SRCDIR)/%.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BINDIR):
	mkdir -p $(BINDIR)

-include $(DEP)

clean:
	rm -f $(BINDIR)/*.o $(BINDIR)/*.d $(TARGET)