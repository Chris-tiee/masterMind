/*
 * Board A: Sender
 */

// Include the given template
#include <templateEMP.h>

//Communication pins
#define STROBE      BIT0    // P1.0  (reuse your CLK pin as STROBE)
#define DATA_PIN1   BIT3    // P1.3
#define DATA_PIN2   BIT4    // P1.4

#define button5     BIT7    // P1.7

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

//Function to pulse the clock of the shift registers
void pulseCK(void){
    P2OUT |= ck;
   __delay_cycles(5);
    P2OUT &= ~ck;
}

volatile int pass[4] = {1,2,3,4}; //to hold the sequence/
//The secret sequence will be a permutation of these four numbers
//These numbers represent the push buttons PB1/PB2/PB3/PB4 respectively

volatile int counter = 0; //to loop over the sequence only used to send and initialize the sequence

volatile int waiting = 1; //to indicate when we are waiting for the players to be ready

volatile int  restart = 0; //to  indicate when we want to restart the game
volatile int  playing = 0; //to indicate if we are in playing mode (1) or waiting mode (0)

//This will be used to save the players guesses while he inputs them during playing mode
volatile int guess[4] = {0,0,0,0};  //to hold the players guesses
volatile int guess_counter = 0;     //to wait for 4 button presses

volatile int won1 = 0; //to indicate if player 1 has won or not
volatile int won2 = 0; //to indicate if player 2 has won or not

//To initialize a random permutation of 4 numbers
static void shuffle4(void)
{
    int j, t;

    j = rand() % 4;
    t = pass[3]; pass[3] = pass[j]; pass[j] = t;

    j = rand() % 3;
    t = pass[2]; pass[2] = pass[j]; pass[j] = t;

    j = rand() % 2;
    t = pass[1]; pass[1] = pass[j]; pass[j] = t;

    serialPrintInt(pass[0]);
    serialPrintInt(pass[1]);
    serialPrintInt(pass[2]);
    serialPrintInt(pass[3]);
    serialPrintln(" ");
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


//After I have loaded in the button states into the shift register
//I will read them one by one
void readButtonInput(){
    int x = 0;
    //Read PB4 output
    if (P2IN & QD1){ //If pressed
        x = 4;
    }else{
        // Set shift register 1 mode to right shift mode (S0 = 1, S1 = 0)
        // In order to read PB3 output
        P2OUT |= s01;
        P2OUT &= ~s11;
        pulseCK(); //shift QC to QD

        //Read PB3 output
        if (P2IN & QD1){ //If pressed
            x = 3;
        }else{
            // Set shift register 1 mode to right shift mode (S0 = 1, S1 = 0)
            // In order to read PB2 output
            P2OUT |= s01;
            P2OUT &= ~s11;
            pulseCK(); //shift QC to QD

            //Read PB2 output
            if (P2IN & QD1){ //If pressed
                x = 2;
            }else{
                // Set shift register 1 mode to right shift mode (S0 = 1, S1 = 0)
                // In order to read PB1 output
                P2OUT |= s01;
                P2OUT &= ~s11;
                pulseCK(); //shift QC to QD

                //Read PB1 output
                if (P2IN & QD1){ //If pressed
                    x = 1;
                }
            }
        }
    }
    if (x != 0){
        int i =0;
        for (i = 0; i < 4; i++){
            if (guess[i] == x){
                x = 0;
                return; //already recorded
            }
        }
        if (x != 0){
            guess[guess_counter] = x;
            guess_counter++;
        }
    }
}

//Interrupt for LED to indicate status
//Flashing if waiting for players to be ready
//On if player can intput the sequence
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

// Main function - this one is called when the chip is connected to the power
int main(void)
{
    initMSP();
    WDTCTL = WDTPW | WDTHOLD;    // stop watchdog
    
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

    //Set up interrupt for status LED_STATUS blink every 0.5s
    TA1CCR0  = 62500 - 1;
    TA1CCTL0 = CCIE;        // enable CCR0 interrupt
    TA1CTL   = TASSEL_2 |ID_3| MC_1 | TACLR; // SMCLK, up mode, clear
    
    serialPrintln("A: waiting for second player...");
    //Wait for the second player to be ready
    while (1){
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
    serialPrintln("A: sending sequence:");
    shuffle4();
    send_sequence(pass);
    __delay_cycles(500000);  // wait ~1 s before starting the game
    __delay_cycles(500000);
    playing = 1;
    restart = 0;

    while (restart == 0){
        if (playing == 1){
            //Stop timer of LED_STATUS and turn LED on
            // This indicates that player is ready to play
            TA1CTL = 0;        // stop timer
            TA1CCTL0 &= ~CCIFG;
            P3OUT |= LED_STATUS; //turn on status LED
            serialPrintln("Input guess");

            int i = 0;
            //initialize guess array and counter
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
            won1 = 1;
            for (i = 0; i <4; i++){
                if (guess[i] != pass[i]){
                    correct[i] = 0;
                    won1 = 0;
                }
            }

            //indicate the visual result of the guess


            //send the result back to the other board
            //just send the won1 status
            if (won1 == 1){
                alpha |= (DATA_PIN1 | DATA_PIN2); //send 11
            }else if (won1 == 0){
                alpha &= ~DATA_PIN2; //send 01
                alpha |= DATA_PIN1;
            }
            send_bits();

            //playing ends here
            //we wait for the other player to send his result
            playing = 0;
            P1DIR &=  ~(DATA_PIN1 | DATA_PIN2); //set data pins to input

        }else{
            serialPrintln("Waiting for other player's result");
            unsigned char bit1 = (P1IN & DATA_PIN1) ? 1 : 0;
            unsigned char bit2 = (P1IN & DATA_PIN2) ? 1 : 0;
            //Check the results
            if (bit1){
                if (bit2 && bit1){
                    won2 = 1;
                }else if (!bit2 && bit1){
                    won2 = 0;
                }
                serialPrintln("Other player's result received");

                P3OUT &= ~(LED_WINNER | LED_LOSER | LED_TIE);
                if (won1 && won2){
                    serialPrintln("Both players WON!");
                    P3OUT |= (LED_TIE);
                    restart = 1; //restart the game after 5s delay
                    waiting = 1; //go back to waiting for the other player to be ready
                }else if (won1 && !won2){
                    serialPrintln("You WON! Other player LOST!");
                    P3OUT |= LED_WINNER;
                    restart = 1; //restart the game after 5s delay
                    waiting = 1; //go back to waiting for the other player to be ready
                }else if (!won1 && won2){
                    serialPrintln("You LOST! Other player WON!");
                    P3OUT |= LED_LOSER;
                    restart = 1; //restart the game after 5s delay
                    waiting = 1; //go back to waiting for the other player to be ready
                }else{
                    serialPrintln("Both players LOST!");
                    P3OUT |= LED_LOSER; //turn on status LED
                    playing = 1; //restart playing
                }
                int i = 0;
                for (i = 0; i<10; i++){
                    __delay_cycles(500000);
                }
                P3OUT &= ~(LED_WINNER | LED_LOSER | LED_TIE);
            }

        }
    }

}


