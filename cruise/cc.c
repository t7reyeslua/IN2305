/**
 * cc.c - Motor speed controller
 *
 * TI2725-C Embedded Software Lab - Day 2
 *
 * Group 11
 *	Student 1: Antonio Reyes 4332431
 *	Student 2: Andres Moreno 4311035
 */

#include "x32.h"
#include "ucos.h"
#include "cc.h"

/*------------------------------------------------------------------
 * isr_buttons -- buttons interrupt service routine
 *------------------------------------------------------------------
 */
void    isr_buttons(void)
{
	int changes;
	OSIntEnter();
	changes = (buttons_flag ^ X32_BUTTONS);

	if (changes & 0x1) {
		if (interrupt_enable_register & BUTTON_INC_ENABLE){
			interrupt_enable_register = interrupt_enable_register & ~BUTTON_INC_ENABLE;
			OSSemPost(sem_button_inc);
		}
	}

	if (changes & 0x2) {
		if (interrupt_enable_register & BUTTON_DEC_ENABLE){
			interrupt_enable_register = interrupt_enable_register & ~BUTTON_DEC_ENABLE;
			OSSemPost(sem_button_dec);
		}
	}

	if (changes & 0x4) {
		if (interrupt_enable_register & BUTTON_ENG_ENABLE){
			interrupt_enable_register = interrupt_enable_register & ~BUTTON_ENG_ENABLE;
			OSSemPost(sem_button_eng);
		}
	}

	if (changes & 0x8) {
		OSSemPost(sem_button_res);

	}
	buttons_flag = X32_BUTTONS;
	OSIntExit();
}

/*------------------------------------------------------------------
 * isr_engine_error
 *------------------------------------------------------------------
 */
void isr_engine_error()
{
	OSIntEnter();
	OSSemPost(sem_error);
	OSIntExit();
}
/*------------------------------------------------------------------
 * decoder_error -- Turn on led 7 on decoder error
 *------------------------------------------------------------------
 */
void decoder_error(void *data)
{
	while(TRUE) {
		OSSemPend(sem_error, WAIT_FOREVER, &err);
		while(OSSemAccept(sem_error) > 0);
		X32_LEDS = (X32_LEDS | LED_ERROR);
		OSTimeDly(5);
		X32_LEDS = (X32_LEDS & ~LED_ERROR);
	}
}

/*------------------------------------------------------------------
 * update_pwm -- Set throttle and shows speed/throttle on display
 *------------------------------------------------------------------
 */
void update_pwm(void *data)
{
	X32_PWM_PERIOD = (int)data;
	while(TRUE) {
		int s = (speed << 4) & 0xff00;
		X32_PWM_WIDTH = throttle;
		if (X32_SWITCHES == 0x00) {
			X32_DISPLAY = (s) | (throttle >> 4);

		}else if (X32_SWITCHES == 0x01) {
			X32_DISPLAY = (speed);

		}else{
			X32_DISPLAY = (throttle);
		}
		OSTimeDly(1);
	}
}

/*------------------------------------------------------------------
 * calculate_speed -- Counts steps in a lapse of time (100ms)
 *------------------------------------------------------------------
 */
void calculate_speed(void *data)
{
	while(TRUE) {
		speed = X32_DECODER - prev_decoder;
		prev_decoder = X32_DECODER;
		OSTimeDly(5);
	}
}

/*------------------------------------------------------------------
 * button_inc -- Increment throttle
 *------------------------------------------------------------------
 */
void button_inc(void *data)
{
	while(TRUE) {
		OSSemPend(sem_button_inc, WAIT_FOREVER, &err);
		OSTimeDly(2);
		if (X32_BUTTONS & 0x01){
			throttle += DELTA_MOTOR;
		}
		interrupt_enable_register = interrupt_enable_register | BUTTON_INC_ENABLE;
	}
}

/*------------------------------------------------------------------
 * button_dec -- Decrement throttle
 *------------------------------------------------------------------
 */
void button_dec(void *data)
{
	while(TRUE) {
		OSSemPend(sem_button_dec, WAIT_FOREVER, &err);
		OSTimeDly(2);
		if (X32_BUTTONS & 0x02){
			throttle -= DELTA_MOTOR;
		}
		interrupt_enable_register = interrupt_enable_register | BUTTON_DEC_ENABLE;
	}
}

/*------------------------------------------------------------------
 * button_eng -- Future engage/disengage
 *------------------------------------------------------------------
 */
void button_eng(void *data)
{
	while(TRUE) {
		OSSemPend(sem_button_eng, WAIT_FOREVER, &err);
		OSTimeDly(2);
		if (X32_BUTTONS & 0x04){
			//
		}
		interrupt_enable_register = interrupt_enable_register | BUTTON_ENG_ENABLE;
	}
}
/*------------------------------------------------------------------
 * button_res -- Exit program
 *------------------------------------------------------------------
 */
void button_res(void *data)
{
	while(TRUE) {
		OSSemPend(sem_button_res, WAIT_FOREVER, &err);
		OSTimeDly(10);
		while(OSSemAccept(sem_button_res) > 0);
		
		DISABLE_INTERRUPT(INTERRUPT_GLOBAL);
		printf("regular exit\r\n");
		exit(0);
	}
}

/*------------------------------------------------------------------
 * main
 *------------------------------------------------------------------
 */
void main()
{
	/* Init variables */
	throttle = 0;
	dec_count = 0;
	speed = 0;
	prev_decoder = 0;
	interrupt_enable_register = 0xFF;
	buttons_flag = 0x0;
	
	/* Define x32 ISR vectors, priorities,etc. */
	DISABLE_INTERRUPT(INTERRUPT_GLOBAL);

	SET_INTERRUPT_VECTOR(INTERRUPT_BUTTONS, &isr_buttons);
	SET_INTERRUPT_PRIORITY(INTERRUPT_BUTTONS, 10);

	SET_INTERRUPT_VECTOR(INTERRUPT_ENGINE_ERROR, &isr_engine_error);
	SET_INTERRUPT_PRIORITY(INTERRUPT_ENGINE_ERROR, 11);

	ENABLE_INTERRUPT(INTERRUPT_ENGINE_ERROR);
	ENABLE_INTERRUPT(INTERRUPT_BUTTONS);


	/* RTOS ucOS section */
	OSInit();
	/* Initialize tasks */
	sem_error = OSSemCreate(0);
	sem_button_inc = OSSemCreate(0);
	sem_button_dec = OSSemCreate(0);
	sem_button_eng = OSSemCreate(0);
	sem_button_res = OSSemCreate(0);

	OSTaskCreate(update_pwm, (void *)1024,
		     (void *)stk_update_pwm, PRIO_UPDATE_PWM);
	OSTaskCreate(decoder_error, (void *)0,
		     (void *)stk_decoder_error, PRIO_DECODER_ERROR);
	OSTaskCreate(calculate_speed, (void *)0,
		     (void *)stk_calculate_speed, PRIO_CALCULATE_SPEED);

	OSTaskCreate(button_inc, (void *)0, (void *)stk_button_inc, PRIO_BUTTON_INC);
	OSTaskCreate(button_dec, (void *)0, (void *)stk_button_dec, PRIO_BUTTON_DEC);
	OSTaskCreate(button_eng, (void *)0, (void *)stk_button_eng, PRIO_BUTTON_ENG);
	OSTaskCreate(button_res, (void *)0, (void *)stk_button_res, PRIO_BUTTON_RES);

	/* Initialize RTOS */
	OSStart();
}
