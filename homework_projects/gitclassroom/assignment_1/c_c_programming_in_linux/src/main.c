#include "temp_conv.h"

int main(int argc, char *argv[])
{
	int opt;
	int conv_num = 0;
	bool ctf = true; /* celcius to fahrenheit flag */
	bool exit_flag = false;
	bool verbose = false;
	
	/* 
	 * if options c and f are both in argv conv_num will be the last
	 * option argument.
	 */
	while ((opt = getopt(argc, argv, "c:f:v")) != -1) {
		switch (opt) {
		case 'c':
			ctf = true;
			conv_num = convInt(optarg, CN_BASE_10, "conv_num, C");
			break;

		case 'f':
			ctf = false;
			conv_num = convInt(optarg, CN_BASE_10, "conv_num, F");
			break;
		
		case 'v':
			verbose = true;
			break;

		default: /* ? */
			errno = EINVAL;
			perror("Invalid option or missing argument");
			exit(EXIT_FAILURE);
		}
	}

	if (argc > 1) {
		/* use command line arguments */
		conv_print(conv_num, ctf);
		exit(EXIT_SUCCESS);
	}

	do {
		/* no arguments, get user input */
		conv_num = menu_input(&ctf, &exit_flag);
		if (errno) {
			perror("menu_input failed to complete");
			exit(EXIT_FAILURE);
		}

		if (!exit_flag)
			conv_print(conv_num, ctf);
	} while (!exit_flag);
	
	exit(EXIT_SUCCESS);
}
