/*
 * User space program to test functionality of basic_char kernel module.
 *
 * written by: James Ross
 */

#include "intel_82540EM_driver_test.h"

int main(void)
{
	int fd;
	int test;
	int exit_flag = EXIT_FALSE;
	led_opts_s led_opts;

	init_led_opts(&led_opts);

	welcome_msg();

	/* open character device */
	fd = open("/dev/intel_82540EM", O_RDWR);
	if (_usrunlikely(errno))
		errExit("open() failed");

	do {
		/* get user input for which test to do */
		test = get_menu_input(&exit_flag, stdin);
		if (_usrunlikely(errno))
			errExit("get_menu_input() failed.");

		/* user wants to exit the program */
		if (_usrunlikely(test == PROG_EXIT))
			exit(EXIT_SUCCESS);

		if (_usrlikely(exit_flag == EXIT_FALSE)) {
			printf("\n");

			/* execute desired test */
			if (test == R_TEST) {
				read_test(fd);
				if (_usrunlikely(errno))
					errExit("read_test execution failed.");
			} else if (test == W_TEST) {
				write_test(fd);
				if (_usrunlikely(errno))
					errExit("write_test execution failed.");
			}
		}
	} while (_usrunlikely(exit_flag != EXIT_TRUE));

	close(fd);
	exit(EXIT_SUCCESS);
}
