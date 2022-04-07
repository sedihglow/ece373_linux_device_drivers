#include "temp_conv.h"

#define BUFF_SIZE   100
#define MSG_SIZE    (100-VERBOSE_LEN)
#define VERBOSE_LEN 10

/* menu input definitions for the menu options */
#define MENU_OPTIONS  3 /* total number of menu options in menu_input() */
#define LOWEST_OPTION 1
#define C_TO_F        1 /* flag for celcius to fahrenheit */
#define F_TO_C        2 /* flag for fahrenheit to celcius */
#define MENU_EXIT     3 /* flag to exit the menu conversion loop */

/* temp conversion equations */
#define CONV_CTF(_conv_num) (((_conv_num)*(9.0/5.0)) + 32) /* C to F */
#define CONV_FTC(_conv_num) (((_conv_num)-32) * (5.0/9.0)) /* F to C */

/* global verbose flag */
static bool verbose = false;

static bool get_verbose()
{
	return verbose;
}

void set_verbose(bool flag)
{
	verbose = flag;
}

static size_t my_strnlen(const char *str, size_t maxlen)
{
	size_t i;
	for (i=0; i < maxlen && str[i]; ++i);
	return i;
}
void print_verbose(const char *fstring, ...)
{
	va_list varg_list;
	char final[BUFF_SIZE] = {'\0'};		/* final msg to be printed */
	char msg[MSG_SIZE] = {'\0'};		/* fstring msg with formating */
	char vs[VERBOSE_LEN] = "[verbose]"; /* verbose tag */

    va_start(varg_list, fstring);

	if (get_verbose()) {
		vsnprintf(msg, MSG_SIZE, fstring, varg_list);
		snprintf(final, BUFF_SIZE, "%s %s", vs, msg);
		printf("%s", final);
	}
	
	va_end(varg_list);
}

/*
 * Gets user input for which conversion to do, prompts for a temperature input
 * then returns that temp value.
 *
 *
 * returns:
 *			- returns in_val as the function return which is the value to convert.
 *			- sets the ctf flag pointer to identify if the in_val is going to be
 *			  C to F or F to C.
 *			- sets exit flag to EXIT_FALSE when we do not want to exit the program.
 *			  sets exit flag to EXIT_TRUE when we want to exit the program.
 *			  sets exit flag to NO_INPUT when no input was given. (EOF signal)
 *			- returns DBL_MAX when exit_flag is set to TRUE and is a clean exit
 * error: 
 *		    - On error, DBL_MIN is returned.
 *
 * NOTE: DBL_MIN is also returned on EOF
 */

double menu_input(bool *ctf, int *exit_flag)
{
	char *input = NULL;
	int in_val = INT_MAX;
	double in_dbl = DBL_MAX;

	if (_usrUnlikely(!ctf || !exit_flag)) {
		errno = EINVAL;
		err_msg("ctf or exit was not allocated before entering.");
		return DBL_MIN;
	}
	
	do {
		printf("\nPlease select which type of unit you want to convert.\n"
			   "1. Celecius to Farhenheit\n"
			   "2. Farhenheit to Celcius\n"
			   "3. exit\n"
			   "Enter number: ");
		fflush(stdout);

		input = get_input(exit_flag);
		if (!input)
			return DBL_MIN;

		in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_ | CN_GT_Z, "in_val, menu");
		if (_usrUnlikely(errno))
			err_msg("failed to convert input");
		
		free(input);
	} while (_usrUnlikely(in_val > MENU_OPTIONS || in_val < LOWEST_OPTION));

	in_dbl = get_usr_temp(in_val, ctf, exit_flag);
	if (errno) {
		err_msg("failed to get temp from input.");
		return DBL_MIN;
	}

	return in_dbl;
}

/*
 * wrapper for fgets_input() that handles the error handling and exit return
 * values.
 *
 */
char* get_input(int *exit_flag)
{
	char *input = NULL;

	input = fgets_input(stdin);
	if (_usrUnlikely(!input)) {
		if (_usrUnlikely(errno)) {
			err_msg("failed to allocate user input");
			*exit_flag = EXIT_TRUE;
			return NULL;
		}
		
		if (ferror(stdin)) {
			errno = EIO;
			err_msg("fgets() failed.");
			*exit_flag = EXIT_TRUE;
			return NULL;
		}

		printf("[warning] no input provided.\n");
		*exit_flag = NO_INPUT;
		return NULL;
	}

	/* if no number was provided and only enter was hit on input */
	if (input[0] == '\0') {
		free(input);
		printf("[warning] no number provided\n");

		*exit_flag = NO_INPUT;
		return NULL;
	}

	return input;
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
char* fgets_input(FILE *fptr)
{
	char buff[BUFF_SIZE] = {'\0'};
	char *input = NULL;
	size_t in_len = 0;

	fgets(buff, BUFF_SIZE, fptr);
	if (_usrLikely(buff[0] != '\0')) {
		in_len = my_strnlen(buff, BUFF_SIZE) - 1; /* set to last index */
		if (_usrLikely(buff[in_len] == '\n'))
			buff[in_len] = '\0';
		else
			clear_stdin();
	} else {
		return NULL;
	}
	
	in_len += 1; /* include full size not index */
	input = CALLOC_ARRAY(char, in_len);
	if (_usrUnlikely(!input)) {
		err_msg("Failed to allocate input array");
		return NULL;
	}
	
	strncpy(input, buff, in_len);

	return input;
}

/*
 * takes a menu input mopt from menu_input() in mopt. ctf and exit flags are
 * set for calling function.
 *
 * returns: DBL_MIN on error or on EOF
 *			in_val on success, temp value given by input
 *			sets ctf flag, celcius to fahrenheit or vice versa.
 *			sets exit_flag for menu_input() function
 *
 *	errors: errno is set on error from functions called.
 */
double get_usr_temp(int mopt, bool *ctf, int *exit_flag)
{
	char *input = NULL;
	double in_val = DBL_MAX;

	if (mopt == C_TO_F) {
		printf("Enter Celcius temperature: ");
		fflush(stdout);

		input = get_input(exit_flag);
		if (!input)
			return DBL_MIN;

		in_val = conv_dbl(input, CN_BASE_10 | CN_NOEXIT_, "in_val, CTF");
		if (_usrUnlikely(errno)) {
			free(input);
			err_msg("failed to convert input");
			*exit_flag = EXIT_TRUE;
			return DBL_MIN;
		}
		
		print_verbose("in_val: %f\n", in_val);

		free(input);

		*exit_flag = EXIT_FALSE;
		*ctf = true;
		return in_val;
	} else if (mopt == F_TO_C) {
		printf("Enter Farenheit temperature: ");
		fflush(stdout);

		input = get_input(exit_flag);
		if (!input)
			return DBL_MIN;

		in_val = conv_dbl(input, CN_BASE_10 | CN_NOEXIT_, "in_val, FTC");
		if (_usrUnlikely(errno)) {
			free(input);
			err_msg("failed to convert input");
			*exit_flag = EXIT_TRUE;
			return DBL_MIN;
		}

		print_verbose("in_val: %f\n", in_val);
		
		free(input);

		*exit_flag = EXIT_FALSE;
		*ctf = false;
		return in_val;
	} else { /* exit */
		*exit_flag = EXIT_TRUE;
		return DBL_MAX;
	}
	return in_val;
}

double conv_print(double conv_num, bool ctf)
{
	double conv_res = 0;

	if (ctf) {
		conv_res = CONV_CTF(conv_num);
		printf("\nCelcius: %0.2f, Farhenheit: %0.2f\n", conv_num, conv_res);
	} else {
		conv_res = CONV_FTC(conv_num);
		printf("\nFarhenheit: %0.2f, Celcius: %0.2f\n", conv_num, conv_res);
	}

	return conv_res;
}
