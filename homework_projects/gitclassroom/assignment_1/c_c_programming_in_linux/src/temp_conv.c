#include "temp_conv.h"

#define BUFF_SIZE 100

/* menu input definitions for the menu options */
#define MENU_OPTIONS 3
#define C_TO_F       1
#define F_TO_C       2
#define MENU_EXIT    3

int menu_input(bool *ctf, bool *exit_flag)
{
	char *input = NULL;
	int in_val;

	if (!ctf || !exit_flag) {
		errno = EINVAL;
		fprintf(stderr, "ctf or exit was not allocated before entering.");
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
		if (errno) {
			fprintf(stderr, "failed to get user input");
			return INT_MIN;
		}

		in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_ | CN_GT_Z, "in_val, menu");
		if (errno) {
			fprintf(stderr, "failed to convert input");
			return INT_MIN;
		}
	} while (in_val > MENU_OPTIONS);

	if (in_val == C_TO_F) {
		printf("Enter Celcius temperature: ");
		fflush(stdout);

		input = fgets_input(stdin);
		if (errno) {
			fprintf(stderr, "failed to get user input");
			return INT_MIN;
		}

		in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_, "in_val, CTF");
		if (errno) {
			fprintf(stderr, "failed to convert input");
			return INT_MIN;
		}
		
		*exit_flag = false;
		*ctf = true;
		return in_val;
	} else if (in_val == F_TO_C) {
		printf("Enter Farenheit temperature: ");
		fflush(stdout);

		input = fgets_input(stdin);
		if (errno) {
			fprintf(stderr, "failed to get user input");
			return INT_MIN;
		}

		in_val = convInt(input, CN_BASE_10 | CN_NOEXIT_, "in_val, FTC");
		if (errno) {
			fprintf(stderr, "failed to convert input");
			return INT_MIN;
		}
		
		*exit_flag = false;
		*ctf = false;
		return in_val;
	} else { /* exit */
		*exit_flag = true;
		return INT_MAX;
	}
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
	in_len = strnlen(buff, BUFF_SIZE) - 1;
	if (buff[in_len] == '\n')
		buff[in_len] = '\0';
	else
		clear_stdin();
	
	in_len += 1; /* include '\0' */
	input = CALLOC_ARRAY(char, in_len);
	if (!input) {
		perror("Failed to allocate input array");
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
		printf("Celcius: %d, Farhenheit: %f\n", conv_num, conv_res);
	} else {
		conv_res = CONV_FTC(conv_num);
		printf("Farhenheit: %d, Celcius: %f\n", conv_num, conv_res);
	}

	return conv_res;
}
