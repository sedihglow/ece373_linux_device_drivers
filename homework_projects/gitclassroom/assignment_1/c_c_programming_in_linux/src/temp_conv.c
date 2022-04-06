#include "temp_conv.h"

#define BUFF_SIZE   100
#define MSG_SIZE    (100-VERBOSE_LEN)
#define VERBOSE_LEN 10

/* menu input definitions for the menu options */
#define MENU_OPTIONS 3
#define C_TO_F       1
#define F_TO_C       2
#define MENU_EXIT    3

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

int menu_input(bool *ctf, int *exit_flag)
{
	char *input = NULL;
	int in_val = INT_MAX;

	if (_usrUnlikely(!ctf || !exit_flag)) {
		errno = EINVAL;
		err_msg("ctf or exit was not allocated before entering.");
		return INT_MIN;
	}
	
	do {
		printf("\nPlease select which type of unit you want to convert.\n"
			   "1. Celecius to Farhenheit\n"
			   "2. Farhenheit to Celcius\n"
			   "3. exit\n"
			   "Enter number: ");
		fflush(stdout);

		input = fgets_input(stdin);
		if (_usrUnlikely(!input)) {
			if (_usrUnlikely(errno)) {
				err_msg("[error] failed to get user input");
				return INT_MIN;
			}
			printf("\n[warning] no input provided.\n");
			*exit_flag = NO_INPUT;
			return INT_MIN;
		}

		in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_ | CN_GT_Z, "in_val, menu");
		if (_usrUnlikely(errno)) {
			free(input);
			err_msg("failed to convert input");
			return INT_MIN;
		}

		print_verbose("in_val: %d\n", in_val);

		free(input);
	} while (_usrUnlikely(in_val > MENU_OPTIONS));

	if (in_val == C_TO_F) {
		printf("Enter Celcius temperature: ");
		fflush(stdout);

		input = fgets_input(stdin);
		if (_usrUnlikely(!input)) {
			if (_usrUnlikely(errno)) {
				err_msg("failed to get user input");
				return INT_MIN;
			}
			printf("\n[warning] no input provided.");
			*exit_flag = NO_INPUT;
			return INT_MIN;
		}

		in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_, "in_val, CTF");
		if (_usrUnlikely(errno)) {
			free(input);
			err_msg("failed to convert input");
			return INT_MIN;
		}
		
		print_verbose("in_val: %d\n", in_val);

		free(input);

		*exit_flag = EXIT_FALSE;
		*ctf = true;
		return in_val;
	} else if (in_val == F_TO_C) {
		printf("Enter Farenheit temperature: ");
		fflush(stdout);

		input = fgets_input(stdin);
		if (_usrUnlikely(!input)) {
			if (_usrUnlikely(errno)) {
				err_msg("failed to get user input");
				return INT_MIN;
			}
			printf("\n[warning] no input provided.\n");
			*exit_flag = NO_INPUT;
			return INT_MIN;
		}

		in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_, "in_val, FTC");
		if (_usrUnlikely(errno)) {
			free(input);
			err_msg("failed to convert input");
			return INT_MIN;
		}

		print_verbose("in_val: %d\n", in_val);
		
		free(input);

		*exit_flag = EXIT_FALSE;
		*ctf = false;
		return in_val;
	} else { /* exit */
		*exit_flag = EXIT_TRUE;
		return INT_MAX;
	}

	return in_val;
}

void clear_stdin()
{
	char ch = '\0';
	while ((ch = getchar()) != '\n' && ch != EOF);
}

char* fgets_input(FILE *fptr)
{
	char buff[BUFF_SIZE] = {'\0'};
	char *input = NULL;
	size_t in_len = 0;

	fgets(buff, BUFF_SIZE, fptr);
	if (_usrUnlikely(buff[0] != '\0')) {
		in_len = strnlen(buff, BUFF_SIZE) - 1; /* set to last index */
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

double conv_print(int conv_num, bool ctf)
{
	double conv_res = 0;

	if (ctf) {
		conv_res = CONV_CTF(conv_num);
		printf("\nCelcius: %d, Farhenheit: %f\n", conv_num, conv_res);
	} else {
		conv_res = CONV_FTC(conv_num);
		printf("\nFarhenheit: %d, Celcius: %f\n", conv_num, conv_res);
	}

	return conv_res;
}
