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
        OSIntEnter();

        switch (X32_BUTTONS){
        	case 0x01: //INCREMENT THROTTLE
                if (interrupt_enable_register & BUTTON_INC_ENABLE){
                    interrupt_enable_register = interrupt_enable_register & ~BUTTON_INC_ENABLE;
                    OSSemPost(sem_button_inc);
                }
        		break;
            case 0x02: //DECREMENT THROTTLE
                if (interrupt_enable_register & BUTTON_DEC_ENABLE){
                    interrupt_enable_register = interrupt_enable_register & ~BUTTON_DEC_ENABLE;
                    OSSemPost(sem_button_dec);
                }
        		break;         	        	
            case 0x04: //DISENGAGE
                if (interrupt_enable_register & BUTTON_ENG_ENABLE){
                    interrupt_enable_register = interrupt_enable_register & ~BUTTON_ENG_ENABLE;
                    OSSemPost(sem_button_eng);
                }
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
        while(OSSemAccept(sem_error) > 0);
        X32_LEDS = (X32_LEDS | LED_ERROR);
        OSTimeDly(5);
        X32_LEDS = (X32_LEDS & ~LED_ERROR);
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

void calculate_speed(void *data) {
    while(TRUE) {
        speed = (X32_DECODER - prev_decoder)/2;
        speed = speed & 0x7fff;
        prev_decoder = X32_DECODER;
        OSTimeDly(5);
     }
}


void cruise_controller(void *data) {
    while(TRUE) {
        if  (control_enable == 1){
            eps = setpoint - speed;
            throttle += eps/4;
        }
        OSTimeDly(2);
     }
}

void cruise_controller_pid(void *data) {
    while(TRUE) {
        eps = setpoint - speed;
        throttle += (P * ((eps - last_eps) + I * eps + D * (eps - 2 * last_eps + lastlast_eps)))/64;

        if (throttle < 0)
            throttle = 0;
        else if (throttle > 1024)
            throttle = 1024;

        lastlast_eps = last_eps;
        last_eps = eps;

        printf("%d %d %d %d\n",++time, setpoint, throttle, speed);
        OSTimeDly(1);
     }
}

void cc_monitor(void *data) {
    while(TRUE) {
        run_monitor();
        OSTimeDly(500);
     }
}


/*------------------------------------------------------------------
 * parallel debouncing of buttons --
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

void button_res(void *data) {
    while(TRUE) {
        OSSemPend(sem_button_res, WAIT_FOREVER, &err);
        OSTimeDly(10);
        while(OSSemAccept(sem_button_res) > 0);
        X32_DISPLAY = 0;
        X32_PWM_WIDTH = 0;
        DISABLE_INTERRUPT(INTERRUPT_GLOBAL);
        printf("regular exit\r\n");
        exit(0);
    }
}

/*------------------------------------------------------------------
 * automatic increment/decrement buttons --
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
    interrupt_enable_register = (char)0xFF;
    control_enable = 0;
    setpoint = 0;
    eps = 0;
    time = 0;
    X32_LEDS = 0;

    P = 1;
    I = 1;
    D = 1;
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

    sem_button_inc_auto = OSSemCreate(0);
    sem_button_dec_auto = OSSemCreate(0);

    OSTaskCreate(update_pwm, (void *)1024, (void *)stk_update_pwm, PRIO_UPDATE_PWM);
    OSTaskCreate(decoder_error, (void *)0, (void *)stk_decoder_error, PRIO_DECODER_ERROR);
    OSTaskCreate(calculate_speed, (void *)0, (void *)stk_calculate_speed, PRIO_CALCULATE_SPEED);
    OSTaskCreate(cruise_controller, (void *)0, (void *)stk_cruise_controller, PRIO_CRUISE_CTR);

    OSTaskCreate(button_inc, (void *)0, (void *)stk_button_inc, PRIO_BUTTON_INC);
    OSTaskCreate(button_dec, (void *)0, (void *)stk_button_dec, PRIO_BUTTON_DEC);
    OSTaskCreate(button_eng, (void *)0, (void *)stk_button_eng, PRIO_BUTTON_ENG);
    OSTaskCreate(button_res, (void *)0, (void *)stk_button_res, PRIO_BUTTON_RES);

    OSTaskCreate(button_inc_auto, (void *)0, (void *)stk_button_inc_auto, PRIO_BUTTON_INC_AUTO);
    OSTaskCreate(button_dec_auto, (void *)0, (void *)stk_button_dec_auto, PRIO_BUTTON_DEC_AUTO);

    //OSTaskCreate(cc_monitor, (void *)0, (void *)stk_monitor, PRIO_MONITOR);

    /*Initialize rtos*/
    OSStart();
}
