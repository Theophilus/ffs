all:
	gcc -g -Wall `pkg-config fuse --cflags --libs` fuse_lfs.c -o flfs