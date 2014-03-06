/**
 * cc.h
 */
#ifndef __IN2305_CC__
#define ___IN2305_CC__

/* define some peripheral short hands
 */
#define X32_DISPLAY				peripherals[PERIPHERAL_DISPLAY]
#define X32_LEDS				peripherals[PERIPHERAL_LEDS]
#define X32_BUTTONS				peripherals[PERIPHERAL_BUTTONS]
#define X32_MS					peripherals[PERIPHERAL_MS_CLOCK] 
#define X32_US					peripherals[PERIPHERAL_US_CLOCK]
#define X32_DECODER				peripherals[PERIPHERAL_ENGINE_DECODED]
#define X32_PWM_PERIOD 			peripherals[PERIPHERAL_DPC1_PERIOD]
#define X32_PWM_WIDTH  			peripherals[PERIPHERAL_DPC1_WIDTH]

#define STACK_SIZE 1024

#define PRIO_UPDATE_PWM			10
#define PRIO_DECODER_ERROR		20
#define PRIO_CALCULATE_SPEED	30

#define PRIO_BUTTON_INC			32
#define PRIO_BUTTON_DEC			33
#define PRIO_BUTTON_ENG			34
#define PRIO_BUTTON_RES			35

 #include "stdint.h"

static unsigned int stk_update_pwm[STACK_SIZE];
static unsigned int stk_decoder_error[STACK_SIZE];
static unsigned int stk_calculate_speed[STACK_SIZE];

static unsigned int stk_button_res[STACK_SIZE];
static unsigned int stk_button_eng[STACK_SIZE];
static unsigned int stk_button_inc[STACK_SIZE];
static unsigned int stk_button_dec[STACK_SIZE];

unsigned char err;
uint16_t throttle;
uint16_t speed;
int dec_count;
char new_a, new_b;
char state_a, state_b;

OS_EVENT *sem_error;
OS_EVENT *sem_button_inc;
OS_EVENT *sem_button_dec;
OS_EVENT *sem_button_eng;
OS_EVENT *sem_button_res;

 #endif      /* __IN2305_CC__ */