/*
// Board B: Receiver
 */

// Include the given template
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

//Function to pulse the clock of the shift registers
void pulseCK(void){
    P2OUT |= ck;
   __delay_cycles(5);
    P2OUT &= ~ck;
}

static int pass[4]; //to hold the incoming shuffled sequence
volatile int counter = 0; //to loop over the sequence

volatile int waiting = 1; //to indicate that we are waiting for the players to be ready

volatile int restart = 1; //to  indicate when we want to restart the game
volatile int playing = 0; //to indicate if the game is ongoing (1) or waiting mode (0)


//This will be used to save the players guesses while he inputs them during playing mode
volatile int guess[4] = {0,0,0,0};  //to hold the players guesses
volatile int guess_counter = 0;     //to wait for 4 button presses

volatile int won1 = 0; //to indicate if player 1 has won or not
volatile int won2 = 0; //to indicate if player 2 has won or not

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

//After I have loaded in the button states into the shift register
//I will read them one by one
void readButtonInput(){
    //Read PB4 output
    if (P2IN & QD1){ //If pressed
        guess[guess_counter] = 4;
        guess_counter++;
    }

    // Set shift register 1 mode to right shift mode (S0 = 1, S1 = 0)
    // In order to read PB3 output
    P2OUT |= s01;
    P2OUT &= ~s11;
    pulseCK(); //shift QC to QD

    //Read PB3 output
    if (P2IN & QD1){ //If pressed
        guess[guess_counter] = 3;
        guess_counter++;
    }

    // Set shift register 1 mode to right shift mode (S0 = 1, S1 = 0)
    // In order to read PB2 output
    P2OUT |= s01;
    P2OUT &= ~s11;
    pulseCK(); //shift QC to QD

    //Read PB2 output
    if (P2IN & QD1){ //If pressed
        guess[guess_counter] = 2;
        guess_counter++;
    }

    // Set shift register 1 mode to right shift mode (S0 = 1, S1 = 0)
    // In order to read PB1 output
    P2OUT |= s01;
    P2OUT &= ~s11;
    pulseCK(); //shift QC to QD

    //Read PB1 output
    if (P2IN & QD1){ //If pressed
        guess[guess_counter] = 1;
        guess_counter++;
    }
}


//Interrupt for LED to indicate status
#pragma vector = TIMER1_A0_VECTOR
__interrupt void Timer1_A0_ISR(void){
    P3OUT ^= LED_STATUS;
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

int main(void){

    // Initialize controller - *ALWAYS* call this *FIRST*. It's in the template,
    // so you have to include that as well. See the lecture notes for details.
    initMSP();

    WDTCTL = WDTPW | WDTHOLD;    // stop watchdog

    // Handshake start:
    // B drives DATA2 (presence), reads DATA1 (ACK), reads STROBE.
    P1DIR |= DATA2;
    P1DIR &= ~(DATA1 | STROBE);
    // Pull-downs to avoid floating when not driven
    P1REN |= (DATA1 | STROBE);
    P1OUT &= ~(DATA1 | STROBE);

    //-------- PB5 as inputs ----------
    P1DIR &= ~button5;       //Set up PB5 as input
    P1REN |= button5 ;        // Enable internal resistor
    P1OUT |= button5 ;        // Select pull-up resistor (so default = HIGH)
    // Set up the interrupt for PB5
    P1IE |= button5 ;      // Enable interrupt
    P1IES |= button5 ;     //flag is set with a high-to-low transition initially
    P1IFG &= ~button5 ;    // Clear interrupt flag

    // Shift Register Pins
    P2DIR |= (clr|s01|s11|s02|s12|ck|sr2);  //outputs
    P2DIR &= ~QD1;                          //input
    P2SEL &= ~(BIT6 | BIT7);    // Make P2.6 and P2.7
    P2SEL2 &= ~(BIT6 | BIT7);   // to normal GPIO pins
    //Reseting all LEDs of shift register to 0
    P2OUT &= ~clr;
    __delay_cycles(10);
    P2OUT |= clr;
    // Set the shift register 2 mode do nothing (S0 = 0, S1 = 0)
    P2OUT &= ~s02;
    P2OUT &= ~s12;
    // Set the shift register 1 mode parallel load mode (S0 = 1, S1 = 1)
    P2OUT |= s01;
    P2OUT |= s11;

    // Set up LEDs
    P3DIR |= (LED_STATUS | LED_WINNER | LED_LOSER | LED_TIE);
    P3OUT &= ~(LED_STATUS | LED_WINNER | LED_LOSER | LED_TIE);

    //Set up interrupt for status LED every 0.5s
    TA1CCR0  = 62500 - 1;
    TA1CCTL0 = CCIE;        // enable CCR0 interrupt
    TA1CTL   = TASSEL_2 |ID_3| MC_1 | TACLR; // SMCLK, up mode, clear

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
    serialPrintln("B: Sequence received.");
    counter = 0;
    restart = 0;

    //Main game loop
    while (restart == 0){
            __delay_cycles(200);          // tiny settle time

        if (playing == 0){
            serialPrintln("Waiting for other player's result");
            unsigned char bit1 = (P1IN & DATA_PIN1) ? 1 : 0;
            unsigned char bit2 = (P1IN & DATA_PIN2) ? 1 : 0;
            if (!bit2 && bit1){
                won1 = 0;
            }else if (bit2 && bit1){
                won1 = 1;
            }
            playing = 1;
            serialPrintln("Other player's result received");

        }else if (playing == 1) {
            //Stop timer of LED_STATUS and turn LED on
            // This indicates that player is ready to play
            TA1CTL = 0;        // stop timer
            TA1CCTL0 &= ~CCIFG;
            P3OUT |= LED_STATUS; //turn on status LED
            serialPrintln("Input guess:");

            //initialize guess array and counter
            int i = 0;
            for (i = 0; i < 4; i++){
                guess[i] = 0;
            }
            guess_counter = 0;

            //I need to wait for the button inputs
            while (guess_counter < 4){
                //Set shift register 1 mode parallel load mode (S0 = 1, S1 = 1)
                P2OUT |= s01;
                P2OUT |= s11;
                pulseCK(); //Load in Q
                //Read PB1/PB2/PB3/PB4 output
                readButtonInput();
            }

            serialPrintln("Guess made:");
            serialPrintInt(guess[0]);
            serialPrintInt(guess[1]);
            serialPrintInt(guess[2]);
            serialPrintInt(guess[3]);
            serialPrintln(" ");

            //after getting 4 inputs, we check if they are correct
            int correct[4] = {1,1,1,1};
            won2 = 1;
            for (i = 0; i <4; i++){
                if (guess[i] != pass[i]){
                    correct[i] = 0;
                    won2 = 0;
                }
            }

            //send the result back to the other board
            //just send the won2 status
            if (won2 == 1){
                P1OUT |= (DATA_PIN1 | DATA_PIN2); //send 11
            }else if (won2 == 0){
                P1OUT &= ~DATA_PIN2; //send 10
                P1OUT |= DATA_PIN1;
            }
        }

    }
}


