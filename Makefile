CPPFLAGS=-std=c++14 -Wall -Wextra -Werror -g -fno-omit-frame-pointer -Iinclude -Iimpl -I../asio-1.10.6/include -DASIO_STANDALONE -Wno-unused-local-typedef -Wno-reorder

LIBS= -lpthread
OBJ=$(patsubst %.cpp,%.o,$(wildcard src/*cpp impl/*cpp) main.cpp)
SRC=$(wildcard src/*cpp impl/*cpp)
HDR=$(wildcard include/*hpp impl/*hpp)

CXX=clang++-3.6

raft.tsk: $(OBJ)
	$(CXX) -o $@ $^ $(CPPFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CPPFLAGS)


.PHONY: clean

format:
	clang-format-3.6 -style google -i $(SRC) $(HDR)
clean:
	rm $(OBJ) raft.tsk
	rm -f $(TESTS) gtest.a gtest_main.a *.o

#testcode
GTEST_DIR = ./googletest/googletest

TEST_CPPFLAGS = -isystem $(GTEST_DIR)/include -Itest
TEST_CXXFLAGS = -g -Wall -Wextra -pthread
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h

.PHONY: test
test : raft_test.tsk

GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

gtest-all.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) $(TEST_CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) $(TEST_CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest-all.cc

gtest_main.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) $(TEST_CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) $(TEST_CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest_main.cc

gtest.a : gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

gtest_main.a : gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

TEST_SRC=$(wildcard test/*cpp)
TEST_HDR=$(wildcard test/*hpp)
TEST_OBJ=$(patsubst %.cpp,%.o,$(wildcard test/*cpp))

raft_test.tsk : $(TEST_SRC) src/raft_server.cpp gtest_main.a
	$(CXX) $(CPPFLAGS) $(TEST_CPPFLAGS) $(CXXFLAGS) $(TEST_CXXFLAGS) -lpthread $^ -o $@
