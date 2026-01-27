/*
 * Board B (SLAVE): announce presence + wait ACK + receive shuffled sequence
 */
#include <templateEMP.h>

//Communication pins
#define STROBE     BIT0    // P1.0
#define DATA1      BIT3    // P1.3
#define DATA2      BIT4    // P1.4

#define LED_STATUS      BIT0        // Yellow   LED on P3.0 to right pin of JP3
#define LED_WINNER      BIT1        // Green    LED on P3.1 to K4
#define LED_LOSER       BIT2        // Red      LED on P3.2 to K3
#define LED_TIE         BIT3        // Blue     LED on P3.3 to X10

#define button5     BIT7    // P1.7

//Shift Register Pins
#define clr BIT5 //P2.5
#define s02 BIT0 //P2.0
#define s12 BIT1 //P2.1
#define s01 BIT2 //P2.2
#define s11 BIT3 //P2.3
#define ck BIT4  //P2.4
#define sr2 BIT6 //P2.6
#define QD1 BIT7 //P2.7

static int pass[4];

static inline void wait_strobe_rising(void)
{
    while (P1IN & STROBE) ;          // wait strobe low
    while (!(P1IN & STROBE)) ;     // wait rising edge
}

static inline int decode_symbol(unsigned char b1, unsigned char b2)
{
    if (!b2 && !b1) return 1;
    if (!b2 &&  b1) return 2;
    if ( b2 && !b1) return 3;
    return 4;
}

// Interrupt functionality of PB5
#pragma vector = PORT1_VECTOR
__interrupt void Port_1(void) {

    P1IE &= ~button5; //Disable Interrupt
    P1IFG &= ~button5; //clear flag

    __delay_cycles(20000); //For debounce window
    // Sample once after debounce window
    unsigned char btn5_pressed = (P1IN & button5) ? 0 : 1;

    //Check which button is pressed (5 or 6)
    if (btn5_pressed) {
        //Stop timer of LED_STATUS and turn LED on
        // This indicates that player is ready to play
        TA1CTL = 0;        // stop timer
        TA1CCTL0 &= ~CCIFG;
        P3OUT |= (LED_TIE | LED_STATUS | LED_WINNER | LED_LOSER);

        __delay_cycles(500000);  // wait ~1 s before restarting the game
        __delay_cycles(500000);
        WDTCTL = 0x0000;
    }

    // Prepare for the next press of the button
    P1IFG &= ~button5;      //clear flag
    P1IE  |=  button5;      // re-enable pin interrupt

}

int main(void)
{
    initMSP();
    WDTCTL = WDTPW | WDTHOLD;

    
    //-------- PB5 as inputs ----------
    P1DIR &= ~button5;       //Set up PB5 as input
    P1REN |= button5 ;        // Enable internal resistor
    P1OUT |= button5 ;        // Select pull-up resistor (so default = HIGH)
    // Set up the interrupt for PB5
    P1IE |= button5 ;      // Enable interrupt
    P1IES |= button5 ;     //flag is set with a high-to-low transition initially
    P1IFG &= ~button5 ;    // Clear interrupt flag

    // Handshake start:
    // B drives DATA2 (presence), reads DATA1 (ACK), reads STROBE.
    P1DIR |= DATA2;
    P1DIR &= ~(DATA1 | STROBE);

    // Pull-downs to avoid floating when not driven
    P1REN |= (DATA1 | STROBE);
    P1OUT &= ~(DATA1 | STROBE);

    serialPrintln("B: announcing presence on DATA2...");
    P1OUT |= DATA2;   // presence = 1

    // Wait for ACK from A on DATA1
    while (!(P1IN & DATA1)) {
        // keep asserting presence until ACK arrives
    }


    serialPrintln("B: ACK received.");
    // Stop driving presence line; switch DATA2 to input with pull-down
    P1OUT &= ~DATA2;
    P1DIR &= ~DATA2;
    P1REN |= DATA2;
    P1OUT &= ~DATA2;

    // Now B will receive 4 symbols:
    serialPrintln("B: receiving sequence:");
    int i = 0;
    // Inputs + pull-downs so nothing floats
    P1DIR &= ~(STROBE | DATA1 | DATA2);
    P1REN |=  (STROBE | DATA1 | DATA2);
    P1OUT &= ~(STROBE | DATA1 | DATA2);   // pull-down

    for (i = 0; i < 4; i++) {
        serialPrintln("B: waiting for STROBE...");
        wait_strobe_rising();
        __delay_cycles(500);  // tiny settle

        unsigned char b1 = (P1IN & DATA1) ? 1 : 0;
        unsigned char b2 = (P1IN & DATA2) ? 1 : 0;

        pass[i] = decode_symbol(b1, b2);
        serialPrintInt(pass[i]);
        serialPrintln(" ");

        while (P1IN & STROBE) ; // wait strobe low
    }


    serialPrintInt(pass[0]); serialPrintInt(pass[1]);
    serialPrintInt(pass[2]); serialPrintInt(pass[3]);
    serialPrintln(" ");
    serialPrintln("B: done.");

    while (1) {
        // idle
    }
}
