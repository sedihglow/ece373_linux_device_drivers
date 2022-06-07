#include "intel_82540EM_driver_test.h"

#define U32_SIZE (sizeof(uint32_t))
#define U8_SIZE  (sizeof(uint8_t))
#define INT_SIZE (sizeof(int))
#define BUFF_SIZE 100

/* main menu definitions */
#define MENU_OPTIONS  3 /* how many options there are in main menu */
#define LOWEST_OPTION 1 /* lowest option value in main menu. */

/* led menu definitions */
#define LED_MENU_OPTS     5
#define LED_MENU_OPTS_MIN 1

#define LED0_MENU 1
#define LED1_MENU 2
#define LED2_MENU 3
#define LED3_MENU 4
#define ALL_LEDS_MENU 5

/* get mode menu defintions */
#define LED_MODE_MENU_OPTS     2
#define LED_MODE_MENU_OPTS_MIN 1

#define LED_MODE_MENU_ON       1
#define LED_MODE_MENU_OFF      2

/* get blink menu definitions */
#define LED_BLINK_MENU_OPTS     2
#define LED_BLINK_MENU_OPTS_MIN 1

#define LED_BLINK_MENU_ON  1
#define LED_BLINK_MENU_OFF 2

static size_t my_strnlen(const char *str, size_t maxlen)
{
	size_t i;
	for (i=0; i < maxlen && str[i]; ++i);
	return i;
}

void init_led_opts(led_opts_s *led_opts)
{
	led_opts->led0_opt = 0;
	led_opts->led1_opt = 0;
	led_opts->led2_opt = 0;
	led_opts->led3_opt = 0;
}

void welcome_msg()
{
	printf("---intel 82540EM pci driver module testing---");
	printf("This program is to run tests on the intel_82540EM kernel module.\n"
	       "Read will read the head and tail of the rx ring into a packed "
	       " value.\n"
	       "Write will write to the blink rate param for timer\n");
}

void clear_stdin()
{
	char ch = '\0';
	while ((ch = getchar()) != '\n' && ch != EOF);
}

/*
 * use fgets to get input. removes newline if needed and clears stdin if needed.
 * allocates memory and returns the pointer to the sized user input string.
 *
 * if only '\n' is provided by the input, the newline is removed and a char is
 * allocated and set to '\0' and returned.
 *
 * if EOF was given, the input buffer will remain set to '\0' and NULL is
 * returned.
 *
 * If a non NULL pointer is returned, the calling program must free the returned
 * pointer.
 *
 * returns: NULL on error or when EOF is provided as the input.
 *			char *input on success, must be freed.
 *
 *	errors: fgets() fails and returns NULL (not at EOF)
 *			calloc() failed to allocate memory, errno is set from calloc().
 */
char *fgets_input(FILE *fptr)
{
	char buff[BUFF_SIZE] = {'\0'};
	char *input = NULL;
	size_t in_len = 0;

	fgets(buff, BUFF_SIZE, fptr);
	if (_usrlikely(buff[0] != '\0')) {
		in_len = my_strnlen(buff, BUFF_SIZE) - 1; /* set to last index */
		if (_usrlikely(buff[in_len] == '\n'))
			buff[in_len] = '\0';
		else
			clear_stdin();
	} else {
		return NULL;
	}

	in_len += 1; /* include full size not index */
	input = CALLOC_ARRAY(char, in_len);
	if (_usrunlikely(!input)) {
		err_msg("Failed to allocate input array");
		return NULL;
	}

	strncpy(input, buff, in_len);

	return input;
}

/*
 * wrapper for fgets_input() that handles the error handling and exit return
 * values.
 *
 * exit_flag is set to EXIT_TRUE on error.
 * exit_flag is set to EXIT_FALSE on success.
 * exit_flag is set to NO_INPUT if '\n' of EOF is provided
 *
 * returns user input string
 */
char *get_input(int *exit_flag, FILE *fptr)
{
	char *input = NULL;

	if (_usrunlikely(!exit_flag || !fptr)) {
		errno = EINVAL;
		errExit("NULL pointer passed to get_input");
	}

	input = fgets_input(fptr);
	if (_usrunlikely(!input)) {
		if (_usrunlikely(errno)) {
			err_msg("failed to allocate user input");
			*exit_flag = EXIT_TRUE;
			return NULL;
		}

		if (_usrunlikely(ferror(fptr))) {
			errno = EIO;
			err_msg("fgets() failed.");
			*exit_flag = EXIT_TRUE;
			return NULL;
		}

		printf("\n[warning] no input provided.\n");

		*exit_flag = NO_INPUT;
		return NULL;
	}

	/* if no number was provided and only enter was hit on input */
	if (_usrunlikely(input[0] == '\0')) {
		free(input);
		printf("[warning] no number provided\n");

		*exit_flag = NO_INPUT;
		return NULL;
	}

	*exit_flag = EXIT_FALSE;
	return input;
}


void print_menu()
{
	printf("\nPlease choose which test to run.\n"
	       "1. read head and tail: value of head and tail via read().\n"
	       "2. write to the blink rate param for timer\n"
	       "3. exit\n"
	       "Enter number: ");
	fflush(stdout);
}

/*
 * print the main test selection menu and get user input
 *
 * returns FAILURE on error
 * returns integer value of user input
 * sets errno on error
 */
int get_menu_input(int *exit_flag, FILE *fptr)
{
	char *input;
	int in_val;

	if (_usrunlikely(!exit_flag || !fptr)) {
		errno = EINVAL;
		errExit("NULL ptr passed to get_menu_input");
	}

	do {
		print_menu();

		input = get_input(exit_flag, fptr);
		/* errno set in get_input */
		if (_usrunlikely(*exit_flag == EXIT_TRUE))
			return FAILURE;

		if (_usrlikely(*exit_flag == EXIT_FALSE)) {
			/* convInt ensures value > 0 */
			in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_ | CN_GT_Z,
					 "in_val, menu");
			if (_usrunlikely(errno)) {
				err_msg("failed to convert input");
				errno = SUCCESS;
				in_val = FAILURE;
			}
		}

		if (_usrlikely(input))
			free(input);
	} while (_usrunlikely(in_val > MENU_OPTIONS)  ||
		 _usrunlikely(*exit_flag == NO_INPUT) ||
		 _usrunlikely(in_val == FAILURE));

	return in_val;
}

int get_mode_option(FILE *fptr)
{
	char *input;
	int in_val;
	int exit_flag = EXIT_FALSE;
	int ret;

	if (_usrunlikely(!fptr)) {
		errno = EINVAL;
		errExit("NULL ptr passed to get_mode_option");
	}

	do {
		printf("\nEnter which mode to set.\n"
		       "1. LED ON\n"
		       "2. LED OFF\n"
		       "Enter number: ");
		fflush(stdout);

		input = get_input(&exit_flag, stdin);
		/* errno set in get_input */
		if (_usrunlikely(exit_flag == EXIT_TRUE))
			return FAILURE;

		if (_usrlikely(exit_flag == EXIT_FALSE)) {
			in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_,
					 "in_val, get_set_val");
			if (_usrunlikely(errno)) {
				err_msg("failed to convert input");
				errno = SUCCESS;
			}
		}

		if (_usrlikely(input))
			free(input);
	} while (_usrunlikely(exit_flag == NO_INPUT)       ||
		 _usrunlikely(in_val > LED_MODE_MENU_OPTS) ||
		 _usrunlikely(in_val < LED_MODE_MENU_OPTS_MIN));

	ret = FAILURE;
	switch (in_val) {
	case LED_MODE_MENU_ON:
		ret = LED_MODE_ON;
	break;
	case LED_MODE_MENU_OFF:
		ret = LED_MODE_OFF;
	break;
	}

	return ret;
}
int get_blink_option(FILE *fptr)
{
	int ret;
	char *input;
	int in_val;
	int exit_flag = EXIT_FALSE;

	if (_usrunlikely(!fptr)) {
		errno = EINVAL;
		errExit("NULL ptr passed to get_blink_option");
	}

	do {
		printf("\nEnter blink setting.\n"
		       "1. LED blink ON\n"
		       "2. LED blink OFF\n"
		       "Enter number: ");
		fflush(stdout);

		input = get_input(&exit_flag, stdin);
		/* errno set in get_input */
		if (_usrunlikely(exit_flag == EXIT_TRUE))
			return FAILURE;

		if (_usrlikely(exit_flag == EXIT_FALSE)) {
			in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_,
					 "in_val, get_set_val");
			if (_usrunlikely(errno)) {
				err_msg("failed to convert input");
				errno = SUCCESS;
			}
		}

		if (_usrlikely(input))
			free(input);
	} while (_usrunlikely(exit_flag == NO_INPUT)       ||
		 _usrunlikely(in_val > LED_BLINK_MENU_OPTS) ||
		 _usrunlikely(in_val < LED_BLINK_MENU_OPTS_MIN));

	ret = FAILURE;
	switch (in_val) {
	case LED_BLINK_MENU_ON:
		ret = LED_BLINK_ON;
	break;
	case LED_MODE_MENU_OFF:
		ret = LED_BLINK_OFF;
	break;
	}

	return ret;
}

int get_led_options(FILE *fptr, led_opts_s *led_opts, unsigned int led_num)
{
	int set_flag;
	uint8_t final_opt = 0;

	if (_usrunlikely(!fptr) || _usrunlikely(!led_opts)) {
		errno = EINVAL;
		errExit("NULL ptr passed to get_led_options");
	}

	if (led_num > NUM_LEDS) {
		errno = EINVAL;
		err_msg("get_led_options invalid led number.");
		return FAILURE;
	}

	set_flag = get_mode_option(stdin);
	switch (set_flag) {
	case LED_MODE_ON:
		final_opt |= LED_SET_MODE;
	break;
	case LED_MODE_OFF:
		final_opt &= ~(LED_SET_MODE);
	break;
	default:
		noerr_msg("get mode returned failure, setting to OFF.");
		final_opt &= ~(LED_SET_MODE);
	}

	set_flag = get_blink_option(stdin);
	switch (set_flag) {
	case LED_BLINK_ON:
		final_opt |= LED_SET_BLINK;
	break;
	case LED_BLINK_OFF:
		final_opt &= ~(LED_SET_BLINK);
	break;
	default:
		noerr_msg("get blink returned failure, setting to OFF.");
		final_opt &= ~(LED_SET_BLINK);
	}

	/* set led option in led_opts struct for specified led */
	switch (led_num) {
	case LED0:
		led_opts->led0_opt = final_opt;
	break;
	case LED1:
		led_opts->led1_opt = final_opt;
	break;

	case LED2:
		led_opts->led2_opt = final_opt;
	break;

	case LED3:
		led_opts->led3_opt = final_opt;
	break;

	case LED_ALL:
		led_opts->led0_opt = final_opt;
		led_opts->led1_opt = final_opt;
		led_opts->led2_opt = final_opt;
		led_opts->led3_opt = final_opt;
	break;
	}

	return SUCCESS;
}
/*
 * prompt the user and get input for which led to be altered.
 *
 * returns int value of user input
 * returns failure on failure
 * errno set on error
 */
int get_led_to_set(FILE *fptr)
{
	int ret;
	char *input;
	int in_val;
	int exit_flag = EXIT_FALSE;

	if (_usrunlikely(!fptr)) {
		errno = EINVAL;
		errExit("NULL ptr passed to get_led_to_set");
	}

	do {
		printf("\nEnter which led to set.\n"
		       "1. LED 0\n"
		       "2. LED 1\n"
		       "3. LED 2\n"
		       "4. LED 3\n"
		       "5. all LEDs\n"
		       "Enter number: ");
		fflush(stdout);

		input = get_input(&exit_flag, stdin);
		/* errno set in get_input */
		if (_usrunlikely(exit_flag == EXIT_TRUE))
			return FAILURE;

		if (_usrlikely(exit_flag == EXIT_FALSE)) {
			in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_,
					 "in_val, get_set_val");
			if (_usrunlikely(errno)) {
				err_msg("failed to convert input");
				errno = SUCCESS;
			}
		}

		if (_usrlikely(input))
			free(input);
	} while (_usrunlikely(exit_flag == NO_INPUT)  ||
		 _usrunlikely(in_val > LED_MENU_OPTS) ||
		 _usrunlikely(in_val < LED_MENU_OPTS_MIN));

	ret = FAILURE;
	switch (in_val) {
	case LED0_MENU:
		ret = LED0;
	break;

	case LED1_MENU:
		ret = LED1;
	break;

	case LED2_MENU:
		ret = LED2;
	break;

	case LED3_MENU:
		ret = LED3;
	break;

	case ALL_LEDS_MENU:
		ret = LED_ALL;
	break;
	}

	return ret;
}

int get_blink_rate(FILE *fptr)
{
	char *input;
	int in_val;
	int exit_flag = EXIT_FALSE;

	if (_usrunlikely(!fptr)) {
		errno = EINVAL;
		errExit("NULL ptr passed to get_led_to_set");
	}

	do {
		printf("\nEnter new blink_rate integer in seconds for LED0.\n"
		       "Enter integer: ");
		fflush(stdout);

		input = get_input(&exit_flag, stdin);
		/* errno set in get_input */
		if (_usrunlikely(exit_flag == EXIT_TRUE))
			return FAILURE;

		if (_usrlikely(exit_flag == EXIT_FALSE)) {
			in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_,
					 "in_val, get_set_val");
			if (_usrunlikely(errno)) {
				err_msg("failed to convert input");
				errno = SUCCESS;
				in_val = FAILURE;
			}
		}

		if (_usrlikely(input))
			free(input);
	} while (_usrunlikely(exit_flag == NO_INPUT) ||
		 _usrunlikely(in_val == FAILURE));

	return in_val;
}

/*
 * test the read system call, read and print the register value returned.
 */
int read_test(int fd)
{
	ssize_t ret;
	int32_t inbuff;
	int16_t head;
	int16_t tail;

	ret = read(fd, &inbuff, U32_SIZE);
	if (_usrunlikely(errno)) {
		err_msg("read_test: read failed");
		return ret;
	}

	if (_usrunlikely(ret < (ssize_t)U32_SIZE)) {
		noerr_msg("read_test: read did not read full bytes");
		return FAILURE;
	}

	head = inbuff >> 16;
	tail = (inbuff << 16) >> 16;

	printf("rx ring tail: %d, rx ring head: %d\n", tail, head);

	return SUCCESS;
}

int write_test(int fd)
{
	int ret;
	int new_blink_rate;

	/* get new blink rate */
	new_blink_rate = get_blink_rate(stdin);
	if (_usrunlikely(errno)) {
		err_msg("wr_test: get_blink_rate error");
		return FAILURE;
	}

	/* write new blink rate to module */
	ret = write(fd, &new_blink_rate, INT_SIZE);
	if (_usrunlikely(errno)) {
		err_msg("write failed in wr_test. ret = %zu", ret);
		return FAILURE;
	}

	printf("Value written to blink rate: %d\n", new_blink_rate);

	return SUCCESS;
}
