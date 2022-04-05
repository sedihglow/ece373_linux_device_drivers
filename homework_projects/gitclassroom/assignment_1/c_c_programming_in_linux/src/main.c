#include "temp_conv.h"

int main(int argc, char *argv[])
{
	int opt;
	int conv_num = 0;
	int conv_res = 0;
	bool ctf = true; /* celcius to farhenheit flag */
	bool exit = false;
	bool verbose = false;
	
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
	do {
		if (argc == 1) {
			/* no arguments, get user input */
			conv_num = menu_input(&ctf, &exit);
			if (errno) {
				perror("menu_input failed to complete");
				exit(EXIT_FAILURE);
			}

			if (!exit)
				conv_print(conv_num, ctf);

		}
	} while (!exit)
	
	if (ctf) {
		conv_res = conv_ctf(conv_num);
		printf("Celcius: %d, Farhenheit: %d\n", conv_num, conv_res);
	} else {
		conv_res = conv_ftc(conv_num);
		printf("Farhenheit: %d, Celcius: %d\n", conv_num, conv_res);
	}

	exit(EXIT_SUCCESS);
}
