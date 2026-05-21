CXX      = c++
STD      = -std=c++17
BREW     = $(shell brew --prefix)
UHC      = $(HOME)/.local

CXXFLAGS = $(STD) -I$(BREW)/include -I$(UHC)/include -Wall -Wno-unused-variable
LDFLAGS  = -L$(BREW)/lib -lCatch2Main -lCatch2 -L$(UHC)/lib -luhc -lpthread

SRCS = test_math.cpp test_matrix.cpp test_list.cpp test_buffer.cpp \
       test_lambda.cpp test_file.cpp test_time.cpp test_threading.cpp test_log.cpp \
       test_network.cpp
OBJS = $(SRCS:.cpp=.o)
BIN  = uhc_test_runner

all: $(BIN)
	./$(BIN)

$(BIN): $(OBJS)
	$(CXX) $(STD) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: $(BIN)
	./$(BIN) --reporter compact

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all run clean
