INCLUDE_DIR = ../../include/
SRC_DIR = ../ittnotify/
SOURCE_NAME = itt_refcol_impl.c
OBJ_NAME = itt_refcol_impl.o
LIB_NAME = libittrefcol.so
CC ?= gcc
CFLAGS = -I$(INCLUDE_DIR) -I$(SRC_DIR)

build:
	$(CC) -fPIC $(CFLAGS) -c $(SOURCE_NAME)
	$(CC) -shared -o $(LIB_NAME) $(OBJ_NAME)

clean:
	 rm $(OBJ_NAME) $(LIB_NAME)
