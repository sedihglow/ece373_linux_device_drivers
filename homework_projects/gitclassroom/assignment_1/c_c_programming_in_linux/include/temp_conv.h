#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <bool.h>
#include "convNum.h"

#define _usrLikely(x)   (__builtin_expect(!!(x), 1))
#define _usrUnlikely(x) (__builtin_expect(!!(x), 0))

#define CALLOC(type)			 ((type*) calloc(1, sizeof(type)))
#define CALLOC_ARRAY(type, size) ((type*) calloc((size), sizeof(type)))

#define SUCCESS 0
#define FAILURE -1

int menu_input(bool *ctf, bool *exit);
void clear_stdin();
char* fgets_input(FILE *fptr);
int conv_ctf(int conv_num);
int conv_ftc(int conv_num);
int conv_either(int conv_num, bool ctf);
