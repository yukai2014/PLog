
###################项目路径和程序名称#################################
DIR=$(shell pwd)
BIN_DIR=$(DIR)/build
LIB_DIR= /home/imdb/supports/glog-0.3.3/.libs/
SRC_DIR=$(DIR)
INCLUDE_DIR=/home/imdb/supports/glog-0.3.3/src/glog
OBJ_DIR=$(DIR)/build
DEPS_DIR=$(DIR)/deps
#PROGRAM=$(BIN_DIR)/test
PROGRAM=$(BIN_DIR)/ProcessLogTest
 
###################OBJ文件及路径############################################
EXTENSION=cpp
OBJS=$(patsubst $(SRC_DIR)/%.$(EXTENSION), $(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.$(EXTENSION)))
DEPS=$(patsubst $(OBJ_DIR)/%.o, $(DEPS_DIR)/%.d, $(OBJS))

###################include头文件路径##################################
INCLUDE=\
        -I$(INCLUDE_DIR)
         
###################lib文件及路径######################################
LIBRARY = \
		-L $(LIB_DIR)
 
###################编译选项及编译器###################################
CC=g++
LDFLAGS= -pthread -std=c++11 -lglog
CXXFLAGS =	-O2 -g -Wall -fmessage-length=0 -std=c++11 
 
###################编译目标###########################################
.PHONY: all clean rebuild
 
all:$(PROGRAM)

$(BIN_DIR)/ProcessLogTest:$(OBJS) 
	$(CC) -o $(BIN_DIR)/ProcessLogTest $(OBJ_DIR)/ProcessLogTest.o $(OBJ_DIR)/logging.o $(LDFLAGS) $(LIBRARY)
 
$(DEPS_DIR)/%.d: $(SRC_DIR)/%.$(EXTENSION)
	$(CC) -MM $(INCLUDE) $(CXXFLAGS) $< | sed -e 1's,^,$(OBJ_DIR)/,' > $@
 
sinclude $(DEPS)
 
$(OBJ_DIR)/%.o:$(SRC_DIR)/%.$(EXTENSION) 
	$(CC) $< -o $@ -c $(CXXFLAGS) $(INCLUDE) 
 
rebuild: clean all
 
clean:
	rm -rf $(OBJS)  $(PROGRAM)