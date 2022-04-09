#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <float.h>
#include "convNum.h"
#include "err_handle.h"

#define _usrLikely(x)   (__builtin_expect(!!(x), 1))
#define _usrUnlikely(x) (__builtin_expect(!!(x), 0))

#define CALLOC(type)			 ((type*) calloc(1, sizeof(type)))
#define CALLOC_ARRAY(type, size) ((type*) calloc((size), sizeof(type)))

#define SUCCESS 0
#define FAILURE -1

/* definitions for the exit_flag in menu_input */
#define EXIT_FALSE 0
#define EXIT_TRUE  1
#define NO_INPUT   2

/* 
 * APPLY_FUNCT() : vectorizes a function funct
 * -Type is the type of pointer used. (VA_ARGS could be void for example.). 
 * -... is a variable argument list.
 * -will execute every argument into the function in order from first arg.
 * -funct only takes in one argument. 
 *
 * BUGS: If a list of pointers is passed and one is NULL before _stopper, the
 *		 function will stop going through the arguments and end.
 *
 *		 if 1 pointer is passed this can be ignored since the function will not
 *		 get called. For example, with free, it will not free the single NULL
 *		 pointer but if its not  NULL and there are more arguments it will call 
 *		 untill it reaches a NULL pointer or the _stopper, whichever is first.
 */
#define APPLY_FUNCT(type, funct, ...)                                          \
({                                                                             \
    void *_stopper = (int[]){0};                                               \
    type **_apply_list = (type*[]){__VA_ARGS__, _stopper};                     \
    int __i_;                                                                  \
                                                                               \
    for (__i_ = 0; _apply_list[__i_] != _stopper; ++__i_)                      \
		(funct)(_apply_list[__i_]);											   \
})
    
/* 
 * apply free to every pointer given in the argument list using the
 * apply_funct macro 
 *
 *
 * BUGS: if a list of pointers is passed using free and one is NULL before the
 *		 stopper it will not free the rest of the pointers given in the 
 *		 argument list since _stopper is NULL.
 */
#define FREE_ALL(...)   (APPLY_FUNCT(void, free, __VA_ARGS__))

void set_verbose(bool flag);
void print_verbose(const char *fstring, ...);

double menu_input(bool *ctf, int *exit_flag);
void clear_stdin();
char* fgets_input(FILE *fptr);
char* get_input(int *exit_flag);
double get_usr_temp(int mopt, bool *ctf, int *exit_flag);

double conv_print(double conv_num, bool ctf);
