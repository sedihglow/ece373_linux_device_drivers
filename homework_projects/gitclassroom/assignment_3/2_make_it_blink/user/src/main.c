/*
 * User space program to test functionality of basic_char kernel module.
 *
 * written by: James Ross
 */

#include "bc_test.h"

int main(void)
{
	int fd;
	int set_val = 15;
	int test;
	int exit_flag = EXIT_FALSE;

	welcome_msg();

	/* open character device */
	fd = open("/dev/basic_char", O_RDWR);
	if (errno)
		errExit("open() failed");

	do {
		/* get user input for which test to do */
		test = get_menu_input(&exit_flag, stdin);
		if (errno)
			errExit("get_menu_input() failed.");

		if (test == PROG_EXIT) /* user wants to exit the program */
			exit(EXIT_SUCCESS);

		if (exit_flag == EXIT_FALSE) {
			/* get value to write to module */
			set_val = get_set_val(stdin);
			if (errno)
				errExit("get_set_val failed.");

			printf("\n");

			/* execute desired test */
			if (test == RWR_SEEK_TEST) {
				rwr_seek_test(fd, set_val);
				if (errno)
					errExit("test execution failed.");
			} else if (test == WRW_NOSEEK_TEST) {
				wrw_no_seek_test(fd, set_val);
				if (errno)
					errExit("test execution failed.");
			}
		}
	} while (exit_flag != EXIT_TRUE);

	close(fd);
	exit(EXIT_SUCCESS);
}
