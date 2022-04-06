#include "temp_conv.h"

int main(int argc, char *argv[])
{
	int opt;
	int conv_num = 0;
	int exit_flag = false;
	bool ctf = true; /* celcius to fahrenheit flag */
	bool use_cmd_args = false;
	
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
			perror("[error] Invalid option or missing argument:");
			exit(EXIT_FAILURE);
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
		if (errno) {
			perror("[error] menu_input failed to complete: ");
			exit(EXIT_FAILURE);
		}

		if (!exit_flag && exit_flag != NO_INPUT)
			conv_print(conv_num, ctf);
	} while (!exit_flag);
	
	exit(EXIT_SUCCESS);
}
