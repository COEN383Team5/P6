CXX			:= gcc
CXXFLAGS	:= -Wall -O3
OBJ_DIR		:= obj/
BIN_DIR		:= bin/
VPATH		:= src:${OBJ_DIR}

P6: P6.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)$@ $(OBJ_DIR)$@.o
.PHONY: P6

clean:
	@rm -rf $(BIN_DIR)
	@rm -rf $(OBJ_DIR)
.PHONY: clean

%.o: %.c
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c -o $(OBJ_DIR)$@ $<

