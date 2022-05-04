#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>
#include "err_handle.h"
#include "convNum.h"

#define _usrlikely(x)   (__builtin_expect(!!(x), 1))
#define _usrunlikely(x) (__builtin_expect(!!(x), 0))

#define CALLOC(type)			 ((type*) calloc(1, sizeof(type)))
#define CALLOC_ARRAY(type, size) ((type*) calloc((size), sizeof(type)))

#define RDWR_SIZE (sizeof(int))


/* definitions for the exit_flag and returns from get_menu_input */
#define EXIT_FALSE 0
#define EXIT_TRUE  1
#define NO_INPUT   2


/* definitions for which test to take, corresponds to menu in print_menu() */
#define R_TEST    1
#define WR_TEST   2
#define PROG_EXIT 3 /* exit the program, no test to take */

#define SUCCESS 0
#define FAILURE -1


/* LED definitions for building led opts to driver and menus */
#define LED0 0
#define LED1 1
#define LED2 2
#define LED3 3
#define LED_ALL 4

#define NUM_LEDS    3 /* counting 0 */
#define MIN_LED_NUM 0

#define LED_MODE_ON   1
#define LED_MODE_OFF  0
#define LED_BLINK_ON  1
#define LED_BLINK_OFF 0

#define LED_SET_MODE  0x01
#define LED_SET_BLINK 0x02

#define LED0_SHIFT 0
#define LED1_SHIFT 2
#define LED2_SHIFT 4
#define LED3_SHIFT 6

#define PACK_LED0(topack) ((topack) << LED0_SHIFT)
#define PACK_LED1(topack) ((topack) << LED1_SHIFT)
#define PACK_LED2(topack) ((topack) << LED2_SHIFT)
#define PACK_LED3(topack) ((topack) << LED3_SHIFT)

typedef struct led_options {
	uint8_t led0_opt;
	uint8_t led1_opt;
	uint8_t led2_opt;
	uint8_t led3_opt;
} led_opts_s;

void init_led_opts(led_opts_s *led_opts);

void welcome_msg();
void print_menu();

int get_menu_input(int *exit_flag, FILE *fptr);
int get_led_to_set(FILE *fptr);
int get_led_options(FILE *fptr, led_opts_s *led_opts, unsigned int led_num);
int get_mode_option(FILE *fptr);
int get_blink_option(FILE *fptr);

char *get_input(int *exit_flag, FILE *fptr);
char *fgets_input(FILE *fptr);

int read_test(int fd);
int wr_test(int fd, led_opts_s *led_opts);
