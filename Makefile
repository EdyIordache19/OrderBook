CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic
LDFLAGS  :=
TARGET   := orderbook
SRC      := main.cpp
SRC      := main.cpp orderbook.cpp orders_generator.cpp orders_pool.cpp
CXXFLAGS += -MMD -MP
DEP      := $(OBJ:.o=.d)
-include $(DEP)
OBJ      := $(SRC:.cpp=.o)

.PHONY: build clean

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)