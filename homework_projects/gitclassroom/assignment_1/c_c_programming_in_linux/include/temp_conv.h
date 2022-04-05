#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "convNum.h"

#define _usrLikely(x)   (__builtin_expect(!!(x), 1))
#define _usrUnlikely(x) (__builtin_expect(!!(x), 0))

#define CALLOC(type)			 ((type*) calloc(1, sizeof(type)))
#define CALLOC_ARRAY(type, size) ((type*) calloc((size), sizeof(type)))

#define SUCCESS 0
#define FAILURE -1

/* temp conversion equations */
#define CONV_CTF(conv_num) (((conv_num)*(9.0/5.0)) + 32) /* C to F */
#define CONV_FTC(conv_num) (((conv_num)-32) * (5.0/9.0)) /* F to C */

int menu_input(bool *ctf, bool *exit_flag);
void clear_stdin();
char* fgets_input(FILE *fptr);
double conv_print(int conv_num, bool ctf);
