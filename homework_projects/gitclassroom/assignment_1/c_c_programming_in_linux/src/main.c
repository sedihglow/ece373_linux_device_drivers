/*
 * Program that takes a celcius or farhrenheit temperature and converted it to
 * the other.
 *
 * Command line arguments can be used instead of user input. If no command line
 * arguments other than -v is active, it will get user input until the user
 * chooses to exit the program.
 *
 * Written by: James Ross
 */

#include "temp_conv.h"

/*
 * command line arguments: $cmd -v for verbose printing.
 *						   $cmd -c [temp] give C temp and convert to F
 *						   $cmd -f [temp] give F temp and convert to C
 */
int main(int argc, char *argv[])
{
	int opt;
	int conv_num = 0;
	int exit_flag = false;	/* control flag for exiting and skipping conv EOF */
	bool ctf = true;		   /* celcius to fahrenheit flag */
	bool use_cmd_args = false; /* use command line arguemnts or not */
	
	/* 
	 * if options c and f are both in argv conv_num will be the last
	 * option argument.
	 */
	while ((opt = getopt(argc, argv, "c:f:v")) != -1) {
		switch (opt) {
		case 'c':
			ctf = true;
			use_cmd_args = true;
			conv_num = convInt(optarg, CN_BASE_10, "conv_num, C");
			break;

		case 'f':
			ctf = false;
			use_cmd_args = true;
			conv_num = convInt(optarg, CN_BASE_10, "conv_num, F");
			break;
		
		case 'v':
			set_verbose(true);
			break;

		default: /* ? */
			errno = EINVAL;
			errExit("Invalid option or missing argument");
		}
	}

	if (use_cmd_args) { 
		/* use command line arguments */
		conv_print(conv_num, ctf);
		exit(EXIT_SUCCESS);
	}

	do {
		/* no arguments, get user input */
		conv_num = menu_input(&ctf, &exit_flag);
		if (errno)
			errExit("menu_input failed to complete: ");

		if (!exit_flag && exit_flag != NO_INPUT)
			conv_print(conv_num, ctf);
	} while (!exit_flag || exit_flag == NO_INPUT);
	
	exit(EXIT_SUCCESS);
}
