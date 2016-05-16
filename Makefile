CPPFLAGS=-std=c++14 -Wall -Wextra -Werror -g -fno-omit-frame-pointer -Iinclude -I../asio-1.11.0/include -DASIO_STANDALONE -Wno-unused-local-typedef -Wno-reorder -fsanitize=memory

LIBS= -lpthread
OBJ=$(patsubst %.cpp,%.o,$(wildcard src/*cpp) main.cpp)
SRC=$(wildcard src/*cpp)
HDR=$(wildcard include/*hpp)

CXX=clang++-3.6

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CPPFLAGS)
raft.tsk: $(OBJ)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LIBS)

.PHONY: clean

format:
	clang-format-3.6 -style google -i $(SRC) $(HDR)
clean:
	rm $(OBJ) raft.tsk
