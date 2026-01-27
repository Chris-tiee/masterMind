/*
 * Board A (MASTER): Presence detect + ACK + send shuffled sequence
 */
#include <templateEMP.h>
#include <stdlib.h>

#define STROBE     BIT0    // P1.0  (reuse your CLK pin as STROBE)
#define DATA1      BIT3    // P1.3
#define DATA2      BIT4    // P1.4

#define button5   BIT7    // P1.7

#define LED_STATUS   BIT0       // Yellow   LED on P3.0 to right pin of JP3
#define LED_WINNER  BIT1        // Green    LED on P3.1 to K4
#define LED_LOSER  BIT2         // Red      LED on P3.2 to K3
#define LED_TIE  BIT3           // Blue     LED on P3.3 to X10

//Shift Register Pins
#define clr BIT5 //P2.5
#define s02 BIT0 //P2.0
#define s12 BIT1 //P2.1
#define s01 BIT2 //P2.2
#define s11 BIT3 //P2.3
#define ck BIT4  //P2.4
#define sr2 BIT6 //P2.6
#define QD1 BIT7 //P2.7

static int pass[4] = {1,2,3,4};

static void init_pass(void)
{
    pass[0] = 1; pass[1] = 2; pass[2] = 3; pass[3] = 4;
}

static void shuffle4(void)
{
    int j, t;

    j = rand() % 4;
    t = pass[3]; pass[3] = pass[j]; pass[j] = t;

    j = rand() % 3;
    t = pass[2]; pass[2] = pass[j]; pass[j] = t;

    j = rand() % 2;
    t = pass[1]; pass[1] = pass[j]; pass[j] = t;
}

static inline unsigned char encode_symbol(int v)
{
    switch (v) {
        case 1: return 0;                 // 00
        case 2: return DATA1;             // 01
        case 3: return DATA2;             // 10
        case 4: return (DATA1 | DATA2);   // 11
        default: return 0;
    }
}

static void send_symbol(int v)
{
    unsigned char bits = encode_symbol(v);

    // Put DATA on bus (ONLY data pins!)
    P1OUT = (P1OUT & ~(DATA1 | DATA2)) | bits;

    __delay_cycles(8000);   // SETUP time (make it large for debugging)

    // STROBE pulse
    P1OUT |= STROBE;
    __delay_cycles(8000);   // HOLD time while strobe high
    P1OUT &= ~STROBE;

    __delay_cycles(8000);   // gap after strobe

    // Go IDLE between symbols (optional but helps a lot)
    P1OUT &= ~(DATA1 | DATA2);
    __delay_cycles(200000); // BIG gap (~200ms) so receiver can never miss
}

void send_sequence(const int pass[4])
{
    int i = 0;
    for (i = 0; i < 4; i++) {
        send_symbol(pass[i]);
    }
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

    // Seed: fixed for now (repeatable). Change later if you want true randomness.
    srand(1234);

    // Direction setup for handshake start:
    // A reads DATA2 (presence) and drives DATA1 (ACK). STROBE is output.
    P1DIR |= (STROBE | DATA1);
    P1DIR &= ~DATA2;

    // Pull-down on DATA2 so it doesn't float while waiting
    P1REN |= DATA2;
    P1OUT &= ~DATA2;

    // Default outputs low
    P1OUT &= ~(STROBE | DATA1);

    //-------- PB5 as inputs ----------
    P1DIR &= ~button5;       //Set up PB5 as input
    P1REN |= button5 ;        // Enable internal resistor
    P1OUT |= button5 ;        // Select pull-up resistor (so default = HIGH)
    // Set up the interrupt for PB5
    P1IE |= button5 ;      // Enable interrupt
    P1IES |= button5 ;     //flag is set with a high-to-low transition initially
    P1IFG &= ~button5 ;    // Clear interrupt flag

    serialPrintln("A: waiting for presence on DATA2...");

    // --- Presence handshake ---
    while (1) {
        if (P1IN & DATA2) {
            serialPrintln("A: presence detected, sending ACK on DATA1");
            P1OUT |= DATA1;            // ACK high
            __delay_cycles(50000);     // hold ACK a bit (~50ms at 1MHz)
            break;
        }
    }

    // --- Send sequence ---
    // For sending, A must drive BOTH data pins + strobe.
    P1DIR |= (DATA1 | DATA2 | STROBE);
    P1OUT &= ~(DATA1 | DATA2 | STROBE);

    init_pass();
    shuffle4();

    serialPrintln("A: sending sequence:");
    serialPrintInt(pass[0]); serialPrintInt(pass[1]);
    serialPrintInt(pass[2]); serialPrintInt(pass[3]);
    serialPrintln(" ");

    send_sequence(pass);

    serialPrintln("A: done sending.");

    while (1) {
        // idle
    }
}
