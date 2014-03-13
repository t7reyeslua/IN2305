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
                if (interrupt_enable_register & 0x01){
                    interrupt_enable_register = interrupt_enable_register & 0xFFFE;
                    OSSemPost(sem_button_inc);
                }
                //printf("%d toggle inc\r\n", n);
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
 * isr_decoder -- software decoder for signals a and b
 * isr for channel ’a’ and ’b’ int new_a, new_b
 *------------------------------------------------------------------
 */
void isr_decoder() {
	OSIntEnter();
	new_a = peripherals[PERIPHERAL_ENGINE_A];
	new_b = peripherals[PERIPHERAL_ENGINE_B];
	
	
	if(new_a != state_a && new_b != state_b) {	// both signals have changed: error
		X32_LEDS = (X32_LEDS | 0x80);
		OSSemPost(sem_error);			
		//printf("post error\r\n");
	}
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


/*------------------------------------------------------------------
 * isr_engine_error --
 *------------------------------------------------------------------
 */
void isr_engine_error() {throttle+=16;
    OSIntEnter();
    OSSemPost(sem_error);
    OSIntExit();
}

void decoder_error(void *data) {
    while(TRUE) {
        OSSemPend(sem_error, WAIT_FOREVER, &err);
        //printf("pend error\r\n");
        while(OSSemAccept(sem_error) > 0);
        X32_LEDS = (X32_LEDS | 0x80);
        OSTimeDly(5);
        X32_LEDS = (X32_LEDS & 0x7F);
    }
}


void update_pwm(void *data) {
	X32_PWM_PERIOD = (int)data;
    while(TRUE) {
        int s = (speed<<4) & 0xff00;

		//PROTECT THROTTLE consistency -MISSING-
		X32_PWM_WIDTH = throttle;
		//printf("pwm_period = %d\r\n",X32_PWM_PERIOD);
		//printf("pwm_width = %d\r\n",X32_PWM_WIDTH);

        if (X32_SWITCHES == 0x00){
            X32_DISPLAY = (s)|(throttle>>4);
        }else if (X32_SWITCHES == 0x01){
            X32_DISPLAY = (speed);
        }else{
            X32_DISPLAY = (throttle);
        }

		//printf("speed = %d\r\n",speed);
		//printf("Throttle: %d\n", throttle);
		//printf("Speed: %d\n", speed);
		OSTimeDly(1);
	}
}


void calculate_speed(void *data) {
    while(TRUE) {
        speed = X32_DECODER - prev_decoder;
        prev_decoder = X32_DECODER;

		//speed = speed>>4;
        X32_DECODER = 0;
        //dec_count = 0;
        OSTimeDly(5);
	}
}

void button_inc(void *data) {
	while(TRUE) {
		OSSemPend(sem_button_inc, WAIT_FOREVER, &err);		
        OSTimeDly(2);

        if (X32_BUTTONS & 0x01){
            throttle += DELTA_MOTOR;
            OSSemPost(sem_button_inc_auto);
        }


        /*//DAY2
        //while(OSSemAccept(sem_button_inc) > 0);
        if (X32_BUTTONS & 0x01){
            throttle += DELTA_MOTOR;
        }
        */

        /*//DAY3 1er intento
        while (X32_BUTTONS & 0x01){
            throttle+=16;
            if ((button_speed & 0x01) == 0){
                button_speed = button_speed | 0x01;
                OSTimeDly(25);
            }else{
                OSTimeDly(1);
            }
        }
        */

        interrupt_enable_register = interrupt_enable_register | 0x0001;

	}
}

void button_inc_auto(void *data) {
    while(TRUE) {
        OSSemPend(sem_button_inc_auto, WAIT_FOREVER, &err);
        OSTimeDly(25);
        while (X32_BUTTONS & 0x01){
            throttle += DELTA_MOTOR;
            OSTimeDly(1);
        }

    }
}

void button_dec(void *data) {
	while(TRUE) {
		OSSemPend(sem_button_dec, WAIT_FOREVER, &err);		
        OSTimeDly(15);
		while(OSSemAccept(sem_button_dec) > 0);
        throttle -= DELTA_MOTOR;
	}
}

void button_eng(void *data) {
	while(TRUE) {
		OSSemPend(sem_button_eng, WAIT_FOREVER, &err);		
        OSTimeDly(15);
		while(OSSemAccept(sem_button_eng) > 0);
	}
}

void button_res(void *data) {
	while(TRUE) {
		OSSemPend(sem_button_res, WAIT_FOREVER, &err);		
        OSTimeDly(15);
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
    interrupt_enable_register = 0xFF;
	/* x32 section********************************/
	
	/**Define x32 ISR vectors, prioritie,etc.*/
	DISABLE_INTERRUPT(INTERRUPT_GLOBAL);

	SET_INTERRUPT_VECTOR(INTERRUPT_BUTTONS, &isr_buttons);
    SET_INTERRUPT_PRIORITY(INTERRUPT_BUTTONS, 10);

    /*
    SET_INTERRUPT_VECTOR(INTERRUPT_ENGINE_A, &isr_decoder);
    SET_INTERRUPT_PRIORITY(INTERRUPT_ENGINE_A, 11);

    SET_INTERRUPT_VECTOR(INTERRUPT_ENGINE_B, &isr_decoder);
    SET_INTERRUPT_PRIORITY(INTERRUPT_ENGINE_B, 11);


    ENABLE_INTERRUPT(INTERRUPT_ENGINE_A);
    ENABLE_INTERRUPT(INTERRUPT_ENGINE_B);
    */

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


    sem_button_inc_auto = OSSemCreate(0);
    sem_button_dec_auto = OSSemCreate(0);
    sem_button_eng_auto = OSSemCreate(0);
    sem_button_res_auto = OSSemCreate(0);

	OSTaskCreate(update_pwm, (void *)1024, (void *)stk_update_pwm, PRIO_UPDATE_PWM);
	OSTaskCreate(decoder_error, (void *)0, (void *)stk_decoder_error, PRIO_DECODER_ERROR);
	OSTaskCreate(calculate_speed, (void *)0, (void *)stk_calculate_speed, PRIO_CALCULATE_SPEED);

	OSTaskCreate(button_inc, (void *)0, (void *)stk_button_inc, PRIO_BUTTON_INC);
	OSTaskCreate(button_dec, (void *)0, (void *)stk_button_dec, PRIO_BUTTON_DEC);
	OSTaskCreate(button_eng, (void *)0, (void *)stk_button_eng, PRIO_BUTTON_ENG);
	OSTaskCreate(button_res, (void *)0, (void *)stk_button_res, PRIO_BUTTON_RES);

    OSTaskCreate(button_inc_auto, (void *)0, (void *)stk_button_inc_auto, PRIO_BUTTON_INC_AUTO);

	/*Initialize rtos*/
	OSStart();
}
