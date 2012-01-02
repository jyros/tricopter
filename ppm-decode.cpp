// Original code from http://pastebin.com/NQtbVCFh
// Posted on the leaflabs.com forum by Dweller:
// http://forums.leaflabs.com/topic.php?id=1170
///**********************************************************************
// Various Maple tests.. including timer capture to memory via dma =)
// */
#include "main.h"
#include "ppm-decode.h"
#include "wirish.h"
#include "usb.h"
#include "timer.h"
#include "dma.h"
#include "utils.h"
#include "yaw-servo.h"

//number of captures to do by dma
#define TIMERS 9

// timer prescale
#define TIMER_PRESCALE 26

// TIMER_PRESCALE*(1/72 MHz) =
#define TICK_PERIOD ( TIMER_PRESCALE*0.0000138888889f )

HardwareTimer timer4(4);// = HardwareTimer(4);
timer_dev *t = TIMER4;
timer_reg_map r = TIMER4->regs;


volatile int dma_data_captured=0;            //set to 1 when dma complete.
volatile uint16 data[TIMERS];   //place to put the data via dma
uint16 delta=0;
uint16 ppm_timeout=0;
int do_print=1;

//dump routine to show content of captured data.
void printData(){
	float duty;
	for(int i=0; i<TIMERS; i++){

		if(ppm_timeout==1){

			if(do_print==1)
			{
				SerialUSB.println("PPM timeout!");
				do_print=0; // print only once for each timeout
			}
			return;
		}

		if(i>0) delta = data[i]-data[i-1];
		else delta = data[i] - data[TIMERS-1];

		duty=(delta)*TICK_PERIOD;

		SerialUSB.print(delta);
		SerialUSB.print(":(");
		SerialUSB.print(duty);
		SerialUSB.print(")");
		if ((i+1)%9==0)	SerialUSB.print("\r");
		else SerialUSB.print("\t");
	}
	SerialUSB.println();
}

//invoked as configured by the DMA mode flags.
void dma_isr()
{
	dma_irq_cause cause = dma_get_irq_cause(DMA1, DMA_CH1);
        //using serialusb to print messages here is nice, but
        //it takes so long, we may never exit this isr invocation
        //before the next one comes in.. (dma is fast.. m'kay)

	timer4.setCount(0);  // clear counter
	if(ppm_timeout) ppm_timeout=0;
	switch(cause)
	{
		case DMA_TRANSFER_COMPLETE:
			// Transfer completed
                        //SerialUSB.println("DMA Complete");
                        dma_data_captured=1;
			break;
		case DMA_TRANSFER_HALF_COMPLETE:
			// Transfer is half complete
                        SerialUSB.println("DMA Half Complete");
			break;
		case DMA_TRANSFER_ERROR:
			// An error occurred during transfer
                        SerialUSB.println("DMA Error");
                        dma_data_captured=1;
			break;
		default:
			// Something went horribly wrong.
			// Should never happen.
                        SerialUSB.println("DMA WTF");
                        dma_data_captured=1;
			break;
	}

}

void ppm_timeout_isr()
{
	// This failure mode indicates we lost comm with
	// the transmitter

	//TODO: re-initialize the dma and wait for ppm signal
	//re-initializing should ensure that the ppm signal
	// is captured on the sync pulse.
	ppm_timeout=1;
	dma_disable(DMA1, DMA_CH1);
    init_ppm_dma_transfer();
}

void init_ppm_timer_and_dma()
{
	init_ppm_timer();
	init_ppm_dma_transfer();
}

void init_ppm_timer()
{

    timer4.pause();
    timer4.setPrescaleFactor(TIMER_PRESCALE);
    timer4.setOverflow(65535);
    timer4.setCount(0);

    // use channel 2 to detect when we stop receiving
    // a ppm signal from the encoder.
    timer4.setMode(TIMER_CH2, TIMER_OUTPUT_COMPARE);
    timer4.setCompare(TIMER_CH2, 65535);
    timer4.attachCompare2Interrupt(ppm_timeout_isr);


    timer4.refresh();

    //capture compare regs TIMx_CCRx used to hold val after a transition on corresponding ICx

    //when cap occurs, flag CCXIF (TIMx_SR register) is set,
    //and interrupt, or dma req can be sent if they are enabled.

    //if cap occurs while flag is already high CCxOF (overcapture) flag is set..

    //CCIX can be cleared by writing 0, or by reading the capped data from TIMx_CCRx
    //CCxOF is cleared by writing 0 to it.

    //Clear the CC1E bit to disable capture from the counter as we set it up.
    //CC1S bits aren't writeable when CC1E is set.
    //CC1E is bit 0 of CCER (page 401)
    bitClear(r.gen->CCER,0);


    //Capture/Compare 1 Selection
    //  set CC1S bits to 01 in the capture compare mode register 1.
    //  01 selects TI1 as the input to use. (page 399 stm32 reference)
    //  (assuming here that TI1 is D16, according to maple master pin map)
    //CC1S bits are bits 1,0
    bitClear(r.gen->CCMR1, 1);
    bitSet(r.gen->CCMR1, 0);


    //Input Capture 1 Filter.
    //  need to set IC1F bits according to a table saying how long
    //  we should wait for a signal to be 'stable' to validate a transition
    //  on the input.
    //  (page 401 stm32 reference)
    //IC1F bits are bits 7,6,5,4
    bitClear(r.gen->CCMR1, 7);
    bitClear(r.gen->CCMR1, 6);
    bitSet(r.gen->CCMR1, 5);
    bitSet(r.gen->CCMR1, 4);

    //sort out the input capture prescaler IC1PSC..
    //00 no prescaler.. capture is done at every edge detected
    bitClear(r.gen->CCMR1, 3);
    bitClear(r.gen->CCMR1, 2);

    //select the edge for the transition on TI1 channel using CC1P in CCER
    //CC1P is bit 1 of CCER (page 401)
    // 0 = rising (non-inverted. capture is done on a rising edge of IC1)
    // 1 = falling (inverted. capture is done on a falling edge of IC1)
    bitClear(r.gen->CCER,1);

    //set the CC1E bit to enable capture from the counter.
    //CC1E is bit 0 of CCER (page 401)
    bitSet(r.gen->CCER,0);

    //enable dma for this timer..
    //sets the Capture/Compare 1 DMA request enable bit on the DMA/interrupt enable register.
    //bit 9 is CC1DE as defined on page 393.
    bitSet(r.gen->DIER,9);
}

void init_ppm_dma_transfer()
{
    dma_init(DMA1);
    dma_setup_transfer( DMA1,    //dma device, dma1 here because that's the only one we have
                        DMA_CH1, //dma channel, channel1, because it looks after tim4_ch1 (timer4, channel1)
                        &(r.gen->CCR1), //peripheral address
                        DMA_SIZE_16BITS, //peripheral size
                        data, //memory address
                        DMA_SIZE_16BITS, //memory transfer size
                        (0
                         //| DMA_FROM_MEM  //set if going from memory, don't set if going to memory.
                         | DMA_MINC_MODE //auto inc where the data does in memory (uses size_16bits to know how much)
                         | DMA_TRNS_ERR  //tell me if it's fubar
                         //| DMA_HALF_TRNS //tell me half way (actually, don't as I spend so long there, I dont see 'complete')
                         | DMA_TRNS_CMPLT //tell me at the end
                         | DMA_CIRC_MODE // circular mode... capture "num_transfers" (below) and repeat
                         )
                        );

    dma_attach_interrupt(DMA1, DMA_CH1, dma_isr); //hook up an isr for the dma chan to tell us if things happen.
    dma_set_num_transfers(DMA1, DMA_CH1, TIMERS); //only allow it to transfer TIMERS number of times.
    dma_enable(DMA1, DMA_CH1);                    //enable it..


}

void print_ppm_data()
{
	//busy wait on the dma_data_captured flag
	//break busy wait if user input is available
	while(!dma_data_captured && !SerialUSB.available());

	do_print=1;
	while(!SerialUSB.available())
	{
	  //dump the data
	  printData();
	  delay(100);
	}


}

void ppm_decode_go()
{


	SerialUSB.print("Starting timer.. rising edge of D");
    SerialUSB.print(PPM_PIN);

	//start the timer counting.
	timer4.resume();
	//the timer is now counting up, and any rising edges on D16
	//will trigger a DMA request to clone the timercapture to the array

}

void ppm_decode(void){
    SerialUSB.println("Decoding DIY Drones PPM encoder on pin D27");

    int pin = 16;
    uint8 prev_state;
    int time_elapsed = 0;
    int time_start = 0;
    int i = 0;
    int channels[8];
    float angle;

    pinMode(pin, INPUT);
    prev_state = (uint8)digitalRead(pin);

    while(!SerialUSB.available()){

        uint8 current_state = (uint8)digitalRead(pin);
        if (current_state != prev_state) {
            if (current_state) {

                time_elapsed = (micros() - time_start);
                time_start = micros();

            }else{
#ifdef USB_VERBOSE
                SerialUSB.print(i);
                SerialUSB.print(":");
                SerialUSB.print(time_elapsed);
#endif
                if(time_elapsed < 2500 && i < 8){
                    if(i==2){
                        if(time_elapsed < 1000){
                            angle = 0.0;
                        }else if(time_elapsed > 2000){
                            angle = 90.0;
                        }else{
                            angle = (float)((time_elapsed-1000)*PPM_CNTS_TO_DEG);
                        }
                        SerialUSB.print(":");
                        SerialUSB.print(angle);
                        set_servo_angle(angle);
                    }
                    channels[i++] = time_elapsed;
                }else{
#ifdef USB_VERBOSE
                    SerialUSB.println("");
#endif
                    i=0;
                }
#ifdef USB_VERBOSE
                SerialUSB.print("\t");
#endif

                time_elapsed = 0;
            }
            prev_state = current_state;
        }

    }
    SerialUSB.println("Done!");

}
