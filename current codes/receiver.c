/*
// Board B: Receiver
 */

// Include the given template
#include <templateEMP.h>

#define CLK_PIN   BIT0      // P1.0
#define DATA_PIN1  BIT3      // P1.3
#define DATA_PIN2  BIT4      // P1.4

#define LED_STATUS   BIT0      // LED on P3.0


volatile int pass[4] = {0,0,0,0}; //to hold the sequence/
volatile int counter = 0; //to loop over the sequence

volatile int waiting = 1; //to indicate when we are waiting for the players to be ready

volatile int  restart = 1; //to  indicate when we want to restart the game
volatile int playing = 0; //to indicate if the game is ongoing
volatile int won1 = 0; //to indicate if player 1 has won or not

void passBuild(int bit1, int bit2, int counter){
    if (!bit2 && !bit1){
        pass[counter] = 1;
    }else if (!bit2 && bit1){
        pass[counter] = 2;
    }else if (bit2 && !bit1){
        pass[counter] = 3;
    }else if (bit2 && bit1){
        pass[counter] = 4;
    }
    serialPrintInt(pass[counter]);
}

//Interrupt for LED to indicate status
#pragma vector = TIMER1_A0_VECTOR
__interrupt void Timer1_A0_ISR(void){
    P3OUT ^= LED_STATUS;
}

int main(void)
{
    // Initialize controller - *ALWAYS* call this *FIRST*. It's in the template,
    // so you have to include that as well. See the lecture notes for details.
    initMSP();

    WDTCTL = WDTPW | WDTHOLD;    // stop watchdog

    // CLOCK and DATA as inputs
    P1DIR &= ~(CLK_PIN | DATA_PIN1);
    P1DIR |= DATA_PIN2; //output at first
    P1OUT |= DATA_PIN2; //default high



    //Set up interrupt for status LED every 0.5s
    TA1CCR0  = 62500 - 1;
    TA1CCTL0 = CCIE;        // enable CCR0 interrupt
    TA1CTL   = TASSEL_2 |ID_3| MC_1 | TACLR; // SMCLK, up mode, clear
    P3DIR |= LED_STATUS;
    P3OUT &= ~LED_STATUS;

    unsigned char last_clk = 0;
     unsigned char clk_now = 0;

    while (1) {

        while (waiting){
            clk_now = (P1IN & CLK_PIN) ? 1 : 0;
            // detect rising edge: last = 0, now = 1
            if (clk_now && !last_clk) {
                // sample DATA on rising edge
                unsigned char bit1 = (P1IN & DATA_PIN1) ? 1 : 0;
                if (bit1 == 1){
                    waiting = 0;
                    P1OUT &= ~DATA_PIN2; //off
                    P1DIR |= DATA_PIN2;
                }
            }
        }


        clk_now = (P1IN & CLK_PIN) ? 1 : 0;

        // detect rising edge: last = 0, now = 1
        if (clk_now && !last_clk) {
            // sample DATA on rising edge
            unsigned char bit1 = (P1IN & DATA_PIN1) ? 1 : 0;
            unsigned char bit2 = (P1IN & DATA_PIN2) ? 1 : 0;

            if (restart == 1){
                if (counter == 3){
                    serialPrintInt(pass[0]);
                    serialPrintInt(pass[1]);
                    serialPrintInt(pass[2]);
                    serialPrintInt(pass[3]);
                    serialPrintln(" ");
                    counter = 0;
                    restart = 0;
                }
                else {
                    passBuild(bit1, bit2, counter);
                    counter++;
                }
            }

            else if (playing == 0){
                if (!bit2 && !bit1){
                    won1 = 0;
                }else if (bit2 && bit1){
                    won1 = 1;
                }
                playing = 1;
                //Stop timer of status LED
                TA0CTL = 0;        // stop timer
                TA0CCTL0 &= ~CCIFG;
                P3OUT |= LED_STATUS; //turn on status LED
            }
        }

        last_clk = clk_now;
    }
}

