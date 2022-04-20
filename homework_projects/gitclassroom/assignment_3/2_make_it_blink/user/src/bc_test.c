#include "bc_test.h"

#define SEEK_START 0
#define BUFF_SIZE 100

static size_t my_strnlen(const char *str, size_t maxlen)
{
	size_t i;
	for (i=0; i < maxlen && str[i]; ++i);
	return i;
}

void welcome_msg()
{
	printf("---basic_char module testing---");
	printf("This program is to run tests on the basic_char kernel module.\n"
	       "rwr_seek_test will call read and write with lseek in between.\n"
	       "wrw_no_seek_test will call write and read without lseek.\n");
}

/*
 * runs the read, write, read test that sets the offset to 0 with lseek between
 * each call to read and write.
 *
 * returns FAILURE on error
 * errno set on error
 */
int rwr_seek_test(int fd, int set_val)
{
	int ret;
	int reading = 0;

	/* read written data from character device */
	ret = read(fd, &reading, RDWR_SIZE);
	if (_usrunlikely(errno)) {
		err_msg("read() failed");
		return FAILURE;
	}

	printf("reading from basic_char: %d, ret = %d\n", reading, ret);

	lseek(fd, SEEK_START, SEEK_SET);
	if (_usrunlikely(errno)) {
		err_msg("lseek() failed");
		return FAILURE;
	}

	printf("lseek set offset to 0\n");

	/* write new data to character device */
	ret = write(fd, &set_val, RDWR_SIZE);
	if (_usrunlikely(errno)) {
		err_msg("write() failed");
		return FAILURE;
	}

	printf("write complete. ret = %d\n", ret);

	lseek(fd, SEEK_START, SEEK_SET);
	if (_usrunlikely(errno)) {
		err_msg("lseek() failed");
		return FAILURE;
	}

	printf("lseek set offset to 0\n");

	/* read written data from character device */
	ret = read(fd, &reading, RDWR_SIZE);
	if (_usrunlikely(errno)) {
		err_msg("read() failed");
		return FAILURE;
	}

	printf("reading from basic_char: %d, ret = %d\n", reading, ret);

	lseek(fd, SEEK_START, SEEK_SET);
	if (_usrunlikely(errno)) {
		err_msg("lseek() failed");
		return FAILURE;
	}

	printf("lseek set offset to 0\n");

	printf("rwr seek test complete\n");

	return SUCCESS;
}

/*
 * runs the write, read, write test that does not call lseek inbetween each
 * call to read and write. No write should be performed and no read should be
 * performed after initial write to the module.
 *
 * returns FAILURE on error
 * errno set on error
 */
int wrw_no_seek_test(int fd, int set_val)
{
	int ret;
	int reading = 0;

	/* write new data to character device */
	ret = write(fd, &set_val, RDWR_SIZE);
	if (_usrunlikely(errno)) {
		err_msg("write() failed");
		return FAILURE;
	}

	printf("write complete. ret = %d\n", ret);

	printf("No lseek between prev write and next read.\n");

	/* try and read data, should return 0 since offset is at end */
	ret = read(fd, &reading, RDWR_SIZE);
	if (_usrunlikely(errno)) {
		err_msg("read() failed");
		return FAILURE;
	}

	printf("reading from basic_char: %d, ret = %d\n", reading, ret);

	printf("No lseek between prev read and next write.\n");

	/*
	 * try to write new data to character device, should not write since
	 * offset is 0
	 */
	ret = write(fd, &set_val, RDWR_SIZE);
	if (_usrunlikely(errno)) {
		err_msg("write() failed");
		return FAILURE;
	}

	printf("write complete. ret = %d\n", ret);

	/* set offset back to 0 for future calls outside of function */
	lseek(fd, SEEK_START, SEEK_SET);
	if (_usrunlikely(errno)) {
		err_msg("lseek() failed");
		return FAILURE;
	}

	printf("lseek set offset to 0\n");

	return SUCCESS;
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
	       "1. rwr_seek_test: read write read with lseek to 0 between.\n"
	       "2. wrw_no_seek_test: write read write with no lseek between.\n"
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

/*
 * prompt the user and get value to write to module during testing.
 *
 * returns int value of user input
 * returns INT_MIN on failure
 * errno set on error
 */
int get_set_val(FILE *fptr)
{
	char *input;
	int in_val;
	int exit_flag = EXIT_FALSE;

	if (!fptr) {
		errno = EINVAL;
		errExit("NULL ptr passed to get_set_val");
	}

	do {
		printf("\nEnter value to use when writing to basic_char: ");
		fflush(stdout);

		input = get_input(&exit_flag, stdin);
		/* errno set in get_input */
		if (_usrunlikely(exit_flag == EXIT_TRUE))
			return INT_MIN;

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
	} while (_usrunlikely(exit_flag == NO_INPUT));

	return in_val;
}
