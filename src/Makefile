CC = g++
AR = ar
RM = rm

CDEBUG = -g
LDEBUG = -g

INC_DIR = ../include
OBJ_DIR = ../obj
LIB_DIR = ../lib

all: $(LIB_DIR)/libCContextFree.a

SRC = \
CContextFree.cpp \
CContextFreeEval.cpp \

OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRC))

CPPFLAGS = \
-std=c++17 \
-I$(INC_DIR) \
-I../../CUtil/include \
-I../../CFile/include \
-I../../CMath/include \
-I../../CRGBName/include \
-I../../CStrUtil/include \
-I../../COS/include \

clean:
	$(RM) -f $(OBJ_DIR)/*.o
	$(RM) -f $(LIB_DIR)/libCContextFree.a

$(OBJS): $(OBJ_DIR)/%.o: %.cpp
	$(CC) -c $< -o $(OBJ_DIR)/$*.o $(CPPFLAGS)

.SUFFIXES: .cpp

$(LIB_DIR)/libCContextFree.a: $(OBJS)
	$(AR) crv $(LIB_DIR)/libCContextFree.a $(OBJS)
