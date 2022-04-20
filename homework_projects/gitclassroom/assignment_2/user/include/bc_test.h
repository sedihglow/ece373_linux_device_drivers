#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "err_handle.h"
#include "convNum.h"

#define _usrlikely(x)   (__builtin_expect(!!(x), 1))
#define _usrunlikely(x) (__builtin_expect(!!(x), 0))

#define CALLOC(type)			 ((type*) calloc(1, sizeof(type)))
#define CALLOC_ARRAY(type, size) ((type*) calloc((size), sizeof(type)))

#define RDWR_SIZE (sizeof(int))


/* definitions for the exit_flag and returns from get_menu_input */
#define EXIT_FALSE 0
#define EXIT_TRUE  1
#define NO_INPUT   2

#define MENU_OPTIONS  3 /* how many options there are in main menu */
#define LOWEST_OPTION 1 /* lowest option value in main menu. */

/* definitions for which test to take, corresponds to menu in print_menu() */
#define RWR_SEEK_TEST   1
#define WRW_NOSEEK_TEST 2
#define PROG_EXIT	3 /* exit the program, no test to take */

#define SUCCESS 0
#define FAILURE -1

void welcome_msg();
void print_menu();

int get_menu_input(int *exit_flag, FILE *fptr);
int get_set_val(FILE *fptr);

char *get_input(int *exit_flag, FILE *fptr);
char *fgets_input(FILE *fptr);

int rwr_seek_test(int fd, int set_val);
int wrw_no_seek_test(int fd, int set_val);
