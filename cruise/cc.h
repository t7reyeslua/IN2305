/**
 * cc.h
 */
#ifndef __IN2305_CC__
#define ___IN2305_CC__

/* define some peripheral short hands
 */
#define X32_DISPLAY			peripherals[PERIPHERAL_DISPLAY]
#define X32_LEDS			peripherals[PERIPHERAL_LEDS]
#define X32_BUTTONS			peripherals[PERIPHERAL_BUTTONS]
#define X32_MS				peripherals[PERIPHERAL_MS_CLOCK]
#define X32_US				peripherals[PERIPHERAL_US_CLOCK]
#define X32_DECODER			peripherals[PERIPHERAL_ENGINE_DECODED]
#define X32_PWM_PERIOD 			peripherals[PERIPHERAL_DPC1_PERIOD]
#define X32_PWM_WIDTH  			peripherals[PERIPHERAL_DPC1_WIDTH]
#define X32_SWITCHES  			peripherals[PERIPHERAL_SWITCHES]

#define STACK_SIZE 1024

#define DELTA_MOTOR 1

#define PRIO_CALCULATE_SPEED		5
#define PRIO_CRUISE_CTR_PID		9
#define PRIO_CRUISE_CTR			10
#define PRIO_UPDATE_PWM			3
#define PRIO_DECODER_ERROR		20
#define PRIO_MONITOR			30

#define PRIO_BUTTON_INC			32
#define PRIO_BUTTON_DEC			33
#define PRIO_BUTTON_ENG			34
#define PRIO_BUTTON_RES			35

#define PRIO_BUTTON_INC_AUTO		36
#define PRIO_BUTTON_DEC_AUTO		37
#define PRIO_BUTTON_ENG_AUTO		38
#define PRIO_BUTTON_RES_AUTO		39

#define BUTTON_INC_ENABLE		0x01
#define BUTTON_DEC_ENABLE		0x02
#define BUTTON_ENG_ENABLE		0x04
#define BUTTON_RES_ENABLE		0x08

#define LED_ERROR       		0x80
#define LED_CC             		0x40

#define TOP_SPEED			1024

#include "stdint.h"

static unsigned int stk_update_pwm[STACK_SIZE];
static unsigned int stk_decoder_error[STACK_SIZE];
static unsigned int stk_calculate_speed[STACK_SIZE];

static unsigned int stk_button_res[STACK_SIZE];
static unsigned int stk_button_eng[STACK_SIZE];
static unsigned int stk_button_inc[STACK_SIZE];
static unsigned int stk_button_dec[STACK_SIZE];

static unsigned int stk_button_inc_auto[STACK_SIZE];
static unsigned int stk_button_dec_auto[STACK_SIZE];

static unsigned int stk_cruise_controller[STACK_SIZE];
static unsigned int stk_cruise_controller_pid[STACK_SIZE];

static unsigned int stk_monitor[STACK_SIZE];

unsigned char err;
int throttle, setpoint;
int control_enable;
int speed, prev_decoder;
int dec_count, eps;
int P, I, D;
int time;
int last_eps, lastlast_eps;
char new_a, new_b;
char state_a, state_b;
char interrupt_enable_register;
int buttons_flag;

OS_EVENT *sem_error;
OS_EVENT *sem_button_inc;
OS_EVENT *sem_button_dec;
OS_EVENT *sem_button_eng;
OS_EVENT *sem_button_res;
OS_EVENT *sem_button_inc_auto;
OS_EVENT *sem_button_dec_auto;

 #endif      /* __IN2305_CC__ */
