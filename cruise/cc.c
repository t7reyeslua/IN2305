/**
 * cc.c - Motor speed controller
 *
 * TI2725-C Embedded Software Lab - Day 3
 *
 * Group 11
 *	Student 1: Antonio Reyes 4332431
 *	Student 2: Andres Moreno 4311035
 */

#include "x32.h"
#include "ucos.h"
#include "cc.h"
#include "monitor.h"

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
void isr_engine_error() {
    OSIntEnter();
    OSSemPost(sem_error);
    OSIntExit();
}

/*------------------------------------------------------------------
 * decoder_error -- Turn on led 7 on decoder error
 *------------------------------------------------------------------
 */
void decoder_error(void *data) {
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
void update_pwm(void *data) {
    X32_PWM_PERIOD = (int)data;
    while(TRUE) {
            int s = (speed<<4) & 0xff00;
            X32_PWM_WIDTH = throttle;

            if (X32_SWITCHES == 0x00){
                X32_DISPLAY = (s)|(throttle>>4);
            }else if (X32_SWITCHES == 0x02){
                X32_DISPLAY = (speed);
            }else if (X32_SWITCHES == 0x01){
                X32_DISPLAY = (throttle);
            }else{
                X32_DISPLAY = (setpoint);
            }
            OSTimeDly(1);
       }
}
/*------------------------------------------------------------------
 * calculate_speed -- Counts steps in a lapse of time (100ms)
 *------------------------------------------------------------------
 */
void calculate_speed(void *data) {
    while(TRUE) {
        speed = (X32_DECODER - prev_decoder)/2;
        speed = speed & 0x7fff;
        prev_decoder = X32_DECODER;
        OSTimeDly(5);
     }
}

/*------------------------------------------------------------------
 * cruise_controller -- Simple cruise controller
 *------------------------------------------------------------------
 */
void cruise_controller(void *data) {
    while(TRUE) {
	    if  (control_enable == 1){

            eps = setpoint - speed;
            throttle += eps/4;
	}
	printf("%d %d %d %d\n",++time, setpoint, throttle, speed);
        OSTimeDly(2);
     }
}

/*------------------------------------------------------------------
 * cruise_controller_pid -- PID cruise controller
 *------------------------------------------------------------------
 */
void cruise_controller_pid(void *data) {
	while(TRUE) {
		if  (control_enable == 1){
			eps = setpoint - speed;
			throttle += (P * ((eps - last_eps) + I * eps + D * (eps - 2 * last_eps + lastlast_eps)))/64;

			if (throttle < 0)
				throttle = 0;
			else if (throttle > 1024)
				throttle = 1024;

			lastlast_eps = last_eps;
			last_eps = eps;

		}
		printf("%d %d %d %d\n",++time, setpoint, throttle, speed);
		OSTimeDly(1);
	}
}

/*------------------------------------------------------------------
 * cc_monitor -- monitoring task
 *------------------------------------------------------------------
 */
void cc_monitor(void *data) {
    while(TRUE) {
        run_monitor();
        OSTimeDly(500);
     }
}


/*------------------------------------------------------------------
 * button_inc -- Increment throttle
 *------------------------------------------------------------------
 */
void button_inc(void *data) {
    while(TRUE) {
        OSSemPend(sem_button_inc, WAIT_FOREVER, &err);
        OSTimeDly(2);
        if (X32_BUTTONS & 0x01){
            if (throttle <= (TOP_SPEED - DELTA_MOTOR)){
                throttle += DELTA_MOTOR;
                setpoint += DELTA_MOTOR;
            }
            if (control_enable == 0)
                OSSemPost(sem_button_inc_auto);
        }
        interrupt_enable_register = interrupt_enable_register | BUTTON_INC_ENABLE;
    }
}

/*------------------------------------------------------------------
 * button_inc_auto -- automatic increment throttle
 *------------------------------------------------------------------
 */
void button_inc_auto(void *data) {
    while(TRUE) {
	OSSemPend(sem_button_inc_auto, WAIT_FOREVER, &err);
	OSTimeDly(25);
	while (X32_BUTTONS & 0x01){
	    if (throttle <= (TOP_SPEED - DELTA_MOTOR))
		throttle += DELTA_MOTOR;
	    OSTimeDly(1);
	}
    }
}

/*------------------------------------------------------------------
 * button_dec -- Decrement throttle
 *------------------------------------------------------------------
 */
void button_dec(void *data) {
    while(TRUE) {
        OSSemPend(sem_button_dec, WAIT_FOREVER, &err);
        OSTimeDly(2);
        if (X32_BUTTONS & 0x02){
            if (throttle >= DELTA_MOTOR){
                throttle -= DELTA_MOTOR;
                setpoint -= DELTA_MOTOR;
            }
            if (control_enable == 0)
                OSSemPost(sem_button_dec_auto);
        }
        interrupt_enable_register = interrupt_enable_register | BUTTON_DEC_ENABLE;
    }
}

/*------------------------------------------------------------------
 * button_dec_auto -- automatic decrement throttle
 *------------------------------------------------------------------
 */
void button_dec_auto(void *data) {
    while(TRUE) {
	OSSemPend(sem_button_dec_auto, WAIT_FOREVER, &err);
	OSTimeDly(25);
	while (X32_BUTTONS & 0x02){
	    if (throttle >= DELTA_MOTOR)
		throttle -= DELTA_MOTOR;
	    OSTimeDly(1);
	}
    }
}

/*------------------------------------------------------------------
 * button_eng -- Future engage/disengage
 *------------------------------------------------------------------
 */
void button_eng(void *data) {
    while(TRUE) {
        OSSemPend(sem_button_eng, WAIT_FOREVER, &err);
        OSTimeDly(2);
        if (X32_BUTTONS & 0x04){
            if (control_enable == 0){
                control_enable = 1;
                setpoint = speed;
                X32_LEDS = (X32_LEDS | LED_ERROR);
            }else{

                control_enable = 0;
                X32_LEDS = (X32_LEDS & ~LED_ERROR);
            }
        }
        interrupt_enable_register = interrupt_enable_register | BUTTON_ENG_ENABLE;
    }
}

/*------------------------------------------------------------------
 * button_res -- Exit program
 *------------------------------------------------------------------
 */
void button_res(void *data) {
    while(TRUE) {
        OSSemPend(sem_button_res, WAIT_FOREVER, &err);
        X32_DISPLAY = 0;
        X32_PWM_WIDTH = 0;
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
    interrupt_enable_register = (char)0xFF;
    control_enable = 0;
    setpoint = 0;
    eps = 0;
    time = 0;
    X32_LEDS = 0;
    buttons_flag;

    P = 15;
    I = 1;
    D = 1;

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

    sem_button_inc_auto = OSSemCreate(0);
    sem_button_dec_auto = OSSemCreate(0);

    OSTaskCreate(update_pwm, (void *)1024, (void *)stk_update_pwm, PRIO_UPDATE_PWM);
    OSTaskCreate(decoder_error, (void *)0, (void *)stk_decoder_error, PRIO_DECODER_ERROR);
    OSTaskCreate(calculate_speed, (void *)0, (void *)stk_calculate_speed, PRIO_CALCULATE_SPEED);
    OSTaskCreate(cruise_controller_pid, (void *)0, (void *)stk_cruise_controller, PRIO_CRUISE_CTR);

    OSTaskCreate(button_inc, (void *)0, (void *)stk_button_inc, PRIO_BUTTON_INC);
    OSTaskCreate(button_dec, (void *)0, (void *)stk_button_dec, PRIO_BUTTON_DEC);
    OSTaskCreate(button_eng, (void *)0, (void *)stk_button_eng, PRIO_BUTTON_ENG);
    OSTaskCreate(button_res, (void *)0, (void *)stk_button_res, PRIO_BUTTON_RES);

    OSTaskCreate(button_inc_auto, (void *)0, (void *)stk_button_inc_auto, PRIO_BUTTON_INC_AUTO);
    OSTaskCreate(button_dec_auto, (void *)0, (void *)stk_button_dec_auto, PRIO_BUTTON_DEC_AUTO);

    //OSTaskCreate(cc_monitor, (void *)0, (void *)stk_monitor, PRIO_MONITOR);

    /* Initialize RTOS */
    OSStart();
}
