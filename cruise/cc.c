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
        		OSSemPost(sem_button_inc);
				//printf("post sem_inc\r\n");
        		break;
        	case 0x02: //DECREMENT THROTTLE
                OSSemPost(sem_button_dec);
				//printf("decrement throttle\r\n");
        		break;         	        	
        	case 0x04: //DISENGAGE
                OSSemPost(sem_button_eng);
        		break;       	
        	case 0x08: //RESET
                OSSemPost(sem_button_res);
        		break;
        	default:
        		break;

        } 

        OSIntExit();
}

/*------------------------------------------------------------------
 * Engine_error
 *------------------------------------------------------------------
 */
void isr_engine_error() {
    OSIntEnter();
    OSSemPost(sem_error);
    OSIntExit();
}

void decoder_error(void *data) {
    while(TRUE) {
        OSSemPend(sem_error, WAIT_FOREVER, &err);
        //printf("pend error\r\n");
        while(OSSemAccept(sem_error) > 0);
        //X32_LEDS = (X32_LEDS | 0x80);
        OSTimeDly(5);
        X32_LEDS = (X32_LEDS & 0x7F);
    }
}

/*------------------------------------------------------------------
 * Speed calculations
 *------------------------------------------------------------------
 */
void update_pwm(void *data) {
    X32_PWM_PERIOD = (int)data;
    while(TRUE) {
            int s = (speed<<4) & 0xff00;
            X32_PWM_WIDTH = throttle;
            if (X32_SWITCHES == 0x00){
                X32_DISPLAY = (s)|(throttle>>4);
            }else if (X32_SWITCHES == 0x01){
                X32_DISPLAY = (speed);
            }else{
                X32_DISPLAY = (throttle);
            }
            OSTimeDly(1);
       }
}

void calculate_speed(void *data) {
	while(TRUE) {	
        speed = X32_DECODER - prev_decoder;
        prev_decoder = X32_DECODER;
        OSTimeDly(5);
     }
}

/*------------------------------------------------------------------
 * parallel debouncing of buttons --
 *------------------------------------------------------------------
 */
void button_inc(void *data) {
	while(TRUE) {
		OSSemPend(sem_button_inc, WAIT_FOREVER, &err);		
		OSTimeDly(10);
		//printf("task button\r\n");
		while(OSSemAccept(sem_button_inc) > 0);
        throttle += DELTA_MOTOR;
		//printf("increment throttle\r\n");
	}
}

void button_dec(void *data) {
	while(TRUE) {
		OSSemPend(sem_button_dec, WAIT_FOREVER, &err);		
		OSTimeDly(10);
		while(OSSemAccept(sem_button_dec) > 0);
        throttle -= DELTA_MOTOR;
	}
}

void button_eng(void *data) {
	while(TRUE) {
		OSSemPend(sem_button_eng, WAIT_FOREVER, &err);		
		OSTimeDly(10);
		while(OSSemAccept(sem_button_eng) > 0);
	}
}

void button_res(void *data) {
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
	/* Init variables*****************************/
	throttle = 0;
	dec_count = 0;
	speed = 0;
    prev_decoder = 0;

	/* x32 section********************************/
	
    /**Define x32 ISR vectors, priorities,etc.*/
	DISABLE_INTERRUPT(INTERRUPT_GLOBAL);

	SET_INTERRUPT_VECTOR(INTERRUPT_BUTTONS, &isr_buttons);
    SET_INTERRUPT_PRIORITY(INTERRUPT_BUTTONS, 10);

    SET_INTERRUPT_VECTOR(INTERRUPT_ENGINE_ERROR, &isr_engine_error);
    SET_INTERRUPT_PRIORITY(INTERRUPT_ENGINE_ERROR, 11);

    ENABLE_INTERRUPT(INTERRUPT_ENGINE_ERROR);
    ENABLE_INTERRUPT(INTERRUPT_BUTTONS);


	/* RTOS ucOS section**************************/
	OSInit();
	/*Initialize tasks*/
	sem_error = OSSemCreate(0);
	sem_button_inc = OSSemCreate(0);
	sem_button_dec = OSSemCreate(0);
	sem_button_eng = OSSemCreate(0);
	sem_button_res = OSSemCreate(0);

	OSTaskCreate(update_pwm, (void *)1024, (void *)stk_update_pwm, PRIO_UPDATE_PWM);
	OSTaskCreate(decoder_error, (void *)0, (void *)stk_decoder_error, PRIO_DECODER_ERROR);
	OSTaskCreate(calculate_speed, (void *)0, (void *)stk_calculate_speed, PRIO_CALCULATE_SPEED);

	OSTaskCreate(button_inc, (void *)0, (void *)stk_button_inc, PRIO_BUTTON_INC);
	OSTaskCreate(button_dec, (void *)0, (void *)stk_button_dec, PRIO_BUTTON_DEC);
	OSTaskCreate(button_eng, (void *)0, (void *)stk_button_eng, PRIO_BUTTON_ENG);
	OSTaskCreate(button_res, (void *)0, (void *)stk_button_res, PRIO_BUTTON_RES);

	/*Initialize rtos*/
	OSStart();
}
