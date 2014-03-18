#include "x32.h"
#include "ucos.h"
#include "cc.h"

/*------------------------------------------------------------------
 * isr_buttons -- buttons interrupt service routine
 *------------------------------------------------------------------
 */
void    isr_buttons(void)
{
        OSIntEnter();

        switch (X32_BUTTONS){
        	case 0x01: //INCREMENT THROTTLE
                throttle += 16;
                //printf("increment throttle\r\n");
        	break;
        	case 0x02: //DECREMENT THROTTLE
                throttle -= 16;
                //printf("decrement throttle\r\n");
        	break;         	        	
        	case 0x04: //DISENGAGE
        	break;       	
        	case 0x08: //RESET
        		DISABLE_INTERRUPT(INTERRUPT_GLOBAL);
                printf("regular exit\r\n");
                exit(0);
        	break;
        	default:
        	break;

        } 

        OSIntExit();
}

/*------------------------------------------------------------------
 * isr_decoder -- software decoder for signals a and b
 * isr for channel ’a’ and ’b’ int new_a, new_b
 *------------------------------------------------------------------
 */
void isr_decoder() {
	OSIntEnter();
	new_a = peripherals[PERIPHERAL_ENGINE_A];
	new_b = peripherals[PERIPHERAL_ENGINE_B];
	
	
	if(new_a != state_a && new_b != state_b) 	// both signals have changed: error
		OSSemPost(sem_error);	
	else if(new_a == 1 && new_b == 1) {			// both signals are high: update up/down counter
		if(state_b == 0) 
			dec_count++;	// b signal comes later: positive direction
		else 
			dec_count--;	// a signal comes later: negative direction
	}

	//update state
	state_a = new_a;
	state_b = new_b;
	OSIntExit();
}


void update_pwm(void *data) {
	X32_PWM_PERIOD = (int)data;
    while(TRUE) {
		X32_PWM_WIDTH = throttle;
        X32_DISPLAY = (speed<<4)|(throttle>>4);
		//printf("pwm_period = %d\r\n",X32_PWM_PERIOD);
        //printf("pwm_width = %d\r\n",X32_PWM_WIDTH);
		//printf("Throttle: %d\n", throttle);
		//printf("Speed: %d\n", speed);
		OSTimeDly(1);
	}
}

void decoder_error(void *data) {
	while(TRUE) {	
		while(OSSemAccept(sem_error) > 0);
		X32_LEDS = (X32_LEDS | 0x80);
		OSTimeDly(5);
		X32_LEDS = (X32_LEDS & 0x7f);
	}
}

void calculate_speed(void *data) {
	while(TRUE) {	
		speed = dec_count;
		//speed = speed>>4;
		dec_count = 0;
		OSTimeDly(5);
	}
}

/*------------------------------------------------------------------
 * main
 *------------------------------------------------------------------
 */
void main()
{
	/* Init variables*****************************/
	throttle = 0;
	dec_count = 0;
	speed = 0;
	/* x32 section********************************/
	
	/**Define x32 ISR vectors, prioritie,etc.*/
	DISABLE_INTERRUPT(INTERRUPT_GLOBAL);

	SET_INTERRUPT_VECTOR(INTERRUPT_BUTTONS, &isr_buttons);
    SET_INTERRUPT_PRIORITY(INTERRUPT_BUTTONS, 10);

    SET_INTERRUPT_VECTOR(INTERRUPT_ENGINE_A, &isr_decoder);
    SET_INTERRUPT_PRIORITY(INTERRUPT_ENGINE_A, 11);

    SET_INTERRUPT_VECTOR(INTERRUPT_ENGINE_B, &isr_decoder);
    SET_INTERRUPT_PRIORITY(INTERRUPT_ENGINE_B, 11);

    ENABLE_INTERRUPT(INTERRUPT_ENGINE_A);
    ENABLE_INTERRUPT(INTERRUPT_ENGINE_B);
    ENABLE_INTERRUPT(INTERRUPT_BUTTONS);


	/* RTOS ucOS section**************************/
	OSInit();
	/*Initialize tasks*/
	sem_error = OSSemCreate(0);
	OSTaskCreate(update_pwm, (void *)1024, (void *)stk_update_pwm, PRIO_UPDATE_PWM);
	OSTaskCreate(decoder_error, (void *)0, (void *)stk_decoder_error, PRIO_DECODER_ERROR);
	OSTaskCreate(calculate_speed, (void *)0, (void *)stk_calculate_speed, PRIO_CALCULATE_SPEED);
	/*Initialize rtos*/
	OSStart();
}
