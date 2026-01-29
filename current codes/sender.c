/*
 * Board A: Sender
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

static inline void wait_strobe_rising(void)
{
    while (P1IN & STROBE);          // wait strobe low
    while (!(P1IN & STROBE));     // wait rising edge
}

volatile int pass[4] = {1,2,3,4}; //to hold the sequence/
//The secret sequence will be a permutation of these four numbers
//These numbers represent the push buttons PB1/PB2/PB3/PB4 respectively

volatile int  restart = 1; //to  indicate when we want to restart the game
volatile int  playing = 0; //to indicate if we are in playing mode (1) or waiting mode (0)

//This will be used to save the players guesses while he inputs them during playing mode
volatile int guess[4] = {0,0,0,0};  //to hold the players guesses

volatile int won1 = 0; //to indicate if player 1 has won or not
volatile int won2 = 0; //to indicate if player 2 has won or not

// ------------ Functions for sending initial sequence ------------
//To initialize a random permutation of 4 numbers
static void shuffle4(void){
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

static inline unsigned char encode_symbol(int v){
    switch (v) {
        case 1: return 0;                 // 00
        case 2: return DATA1;             // 01
        case 3: return DATA2;             // 10
        case 4: return (DATA1 | DATA2);   // 11
        default: return 0;
    }
}

static void send_symbol(int v){
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

void send_sequence(const int pass[4]){
    int i = 0;
    for (i = 0; i < 4; i++) {
        send_symbol(pass[i]);
    }
}

//----------- Functions to read the button inputs -----------
//After I have loaded in the button states into the shift register
//I will read them one by one
// returns 4-bit mask: b0=PB1, b1=PB2, b2=PB3, b3=PB4
unsigned char read_buttons_mask(void){
    unsigned char m = 0;

    // 1) parallel load the register (you already do this)
    P2OUT |= s01; P2OUT |= s11;   // parallel load mode
    pulseCK();                    // load button states into SR

    // Now QD1 reads PB4 first in your wiring (adjust if needed)
    // We'll read PB4, PB3, PB2, PB1 by shifting and sampling QD1

    // PB4
    if (P2IN & QD1) m |= (1 << 3);

    // shift -> PB3 to QD
    P2OUT |= s01; P2OUT &= ~s11;  // right shift mode
    pulseCK();
    if (P2IN & QD1) m |= (1 << 2);

    // PB2
    pulseCK();
    if (P2IN & QD1) m |= (1 << 1);

    // PB1
    pulseCK();
    if (P2IN & QD1) m |= (1 << 0);

    return m;
}

// converts one-hot mask bit to number 1..4
int mask_to_button(unsigned char onehot){
    if (onehot & (1<<0)) return 1;
    if (onehot & (1<<1)) return 2;
    if (onehot & (1<<2)) return 3;
    if (onehot & (1<<3)) return 4;
    return 0;
}

//Now the functions to get the inputs
void collect_guess4(void){
    int i = 0;
    for (i = 0; i < 4; i++) guess[i]=0;

    int guess_counter = 0;
    unsigned char prev = 0;

    while (guess_counter < 4) {
        unsigned char m = read_buttons_mask();

        unsigned char new_press = (unsigned char)(m & ~prev);

        if (new_press) {
            // If multiple pressed at once, ignore until only one is pressed
            if ((new_press & (new_press - 1)) == 0) { // power-of-two check
                int b = mask_to_button(new_press);

                // Optional: reject duplicates (if your rules want that)
                int dup = 0;
                int k = 0;
                for (k=0;k<guess_counter;k++) if (guess[k]==b) dup=1;
                if (!dup) {
                    guess[guess_counter++] = b;
                }

                // Wait until ALL buttons released (simple debounce)
                do {
                    __delay_cycles(20000);
                    m = read_buttons_mask();
                } while (m != 0);
            }
        }
        prev = m;
        __delay_cycles(5000); // small scan period
    }
}

// ------------ Function for sending result ------------
void send_result(int win){
    //Outputs
    P1DIR |= (STROBE|DATA1|DATA2);
    P1REN &=  ~(STROBE|DATA1|DATA2);
    P1OUT &= ~(STROBE|DATA1|DATA2);

    // LOSE = 10 (DATA1=1, DATA2=0), WIN = 11
    unsigned char bits = DATA1 | (win ? DATA2 : 0);

    P1OUT = (P1OUT & ~(DATA1|DATA2)) | bits;
    __delay_cycles(20000);

    P1OUT |= STROBE;
    __delay_cycles(20000);
    P1OUT &= ~STROBE;

    __delay_cycles(8000);   // gap after strobe
    
    // go idle
    P1OUT &= ~(DATA1|DATA2);
    __delay_cycles(200000); // BIG gap (~200ms) so receiver can never miss
}
// ------------ Function for receiving result ------------
int recv_result(void){
    //Inputs with pull-downs
    P1DIR &= ~(STROBE|DATA1|DATA2);
    P1REN |=  (STROBE|DATA1|DATA2);
    P1OUT &= ~(STROBE|DATA1|DATA2); // pull-down
    // serialPrintln("Waiting for strobe...");
    wait_strobe_rising();
    __delay_cycles(500);
    
    // serialPrintln("Strobe received...");

    int b1 = (P1IN & DATA1) ? 1 : 0;
    int b2 = (P1IN & DATA2) ? 1 : 0;

    while (P1IN & STROBE) ;
    // serialPrintln("Strobe ended.");

    // only accept messages with DATA1=1 (valid)
    if (!b1) {
        // serialPrintln("received r: -1");
        return -1;  // invalid/noise
    }
    int r = b2 ? 1 : 0;
    serialPrintln("received r:");
    serialPrintInt(r);
    serialPrintln(" ");
    return (b2 ? 1 : 0); // b2=1 => WIN, else LOSE
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

    //-------- PB5 as input for Restart Button ----------
    P1DIR &= ~button5;       //Set up PB5 as input
    P1REN |= button5 ;        // Enable internal resistor
    P1OUT |= button5 ;        // Select pull-up resistor (so default = HIGH)
    // Set up the interrupt for PB5
    P1IE |= button5 ;      // Enable interrupt
    P1IES |= button5 ;     //flag is set with a high-to-low transition initially
    P1IFG &= ~button5 ;    // Clear interrupt flag

    // ---------- Shift Register Pins -----------
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
    
    // ---------- Set up LEDs for status indication -----------
    P3DIR |= (LED_STATUS | LED_WINNER | LED_LOSER | LED_TIE);

    while (restart == 1){

        // ---------- Set up LEDs for status indication -----------
        P3OUT &= ~(LED_STATUS | LED_WINNER | LED_LOSER | LED_TIE);

        //Set up interrupt for status LED_STATUS blink every 0.5s
        TA1CCR0  = 62500 - 1;
        TA1CCTL0 = CCIE;        // enable CCR0 interrupt
        TA1CTL   = TASSEL_2 |ID_3| MC_1 | TACLR; // SMCLK, up mode, clear
    
        // Direction setup for handshake start:
        // A reads DATA2 (presence) and drives DATA1 (ACK). STROBE is output.
        P1SEL  &= ~(STROBE|DATA1|DATA2);
        P1SEL2 &= ~(STROBE|DATA1|DATA2);

        P1DIR |= (STROBE | DATA1);  // Outputs
        P1DIR &= ~DATA2;            // Input
        // Pull-down on DATA2 so it doesn't float while waiting
        P1REN |= DATA2;
        P1REN &=  ~(STROBE|DATA1);
        P1OUT &= ~DATA2;
        // Default outputs low
        P1OUT &= ~(STROBE | DATA1);
        // ------------- Handshake -------------
        serialPrintln("Master: waiting for second player...");
        //Wait for the second player to be ready
        while (1){
            if (P1IN & DATA2) {
                serialPrintln("Master: presence detected, sending ACK on DATA1");
                P1OUT |= DATA1;            // ACK high
                __delay_cycles(200000);     // hold ACK a bit (~200ms at 1MHz)
                break;
            }
        }

        // ------------- Send shuffled sequence -------------
        //Innitialize communication pins for sending sequence
        P1DIR |= (DATA1 | DATA2 | STROBE);
        P1OUT &= ~(DATA1 | DATA2 | STROBE);
        serialPrintln("Master: sending sequence:");
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
                serialPrintln("Input guess: ");

                collect_guess4();

                serialPrintln("Guess made:");
                serialPrintInt(guess[0]);
                serialPrintInt(guess[1]);
                serialPrintInt(guess[2]);
                serialPrintInt(guess[3]);
                serialPrintln(" ");

                //after getting 4 inputs, we check if they are correct
                int correct[4] = {1,1,1,1};
                won1 = 1;
                int i = 0;
                for (i = 0; i <4; i++){
                    if (guess[i] != pass[i]){
                        correct[i] = 0;
                        won1 = 0;
                    }
                }
                
                //indicate the visual result of the guess


                //send the result back to the other board
                //just send the won1 status
                send_result(won1);
                

                //playing ends here
                //we wait for the other player to send his result
                playing = 0;
                
                //Turn on timer for status LED again
                TA1CCR0  = 62500 - 1;
                TA1CCTL0 = CCIE;        // enable CCR0 interrupt
                TA1CTL   = TASSEL_2 |ID_3| MC_1 | TACLR; // SMCLK, up mode, clear

            }else{
                serialPrintln("Waiting for other player's result");
                int r = -1;
                do { r = recv_result(); } while (r < 0);
                won2 = r;
                serialPrintln("Other player's result received");

                P3OUT &= ~(LED_WINNER | LED_LOSER | LED_TIE);
                if (won1 && won2){
                    serialPrintln("Both players WON!");
                    P3OUT |= (LED_TIE);
                    restart = 1; //restart the game after 5s delay
                }else if (won1 && !won2){
                    serialPrintln("You WON! Other player LOST!");
                    P3OUT |= LED_WINNER;
                    restart = 1; //restart the game after 5s delay
                }else if (!won1 && won2){
                    serialPrintln("You LOST! Other player WON!");
                    P3OUT |= LED_LOSER;
                    restart = 1; //restart the game after 5s delay
                }else{
                    serialPrintln("Both players LOST!");
                    P3OUT |= LED_LOSER; //turn on status LED
                    playing = 1; //restart playing in same game
                }
                int i = 0;
                for (i = 0; i<10; i++){
                    __delay_cycles(500000);
                }
                P3OUT &= ~(LED_WINNER | LED_LOSER | LED_TIE);
            }
        }
         __delay_cycles(500000);
        serialPrintln("A new game will start now...");
    }
}


