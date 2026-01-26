/*
 * Board A: Sender
 */

// Include the given template
#include <templateEMP.h>

//Communication pins
#define CLK_PIN   BIT0      // P1.0
#define DATA_PIN1  BIT3      // P1.3
#define DATA_PIN2  BIT4      // P1.4

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

//Function to pulse the clock of the shift registers
void pulseCK(void){
    P2OUT |= ck;
   __delay_cycles(5);
    P2OUT &= ~ck;
}

volatile int pass[4] = {1,2,3,4}; //to hold the sequence/
//The secret sequence will be a permutation of these four numbers
//These numbers represent the push buttons PB1/PB2/PB3/PB4 respectively

//This will be used to set the state of the data pins sent
//So technically it holds the value of the data pins sent
//Basically I only use it to avoid changing P1OUT directly all the time
//It will hold the values of DATA_PIN1 and DATA_PIN2
unsigned char alpha = 0; //to set the state of the data pins sent
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
void shuffle4(){
    // i = 3
    int j = rand() % 4;
    int t = pass[3]; pass[3] = pass[j]; pass[j] = t;

    // i = 2
    j = rand() % 3;
    t = pass[2]; pass[2] = pass[j]; pass[j] = t;

    // i = 1
    j = rand() % 2;
    t = pass[1]; pass[1] = pass[j]; pass[j] = t;

    serialPrintInt(pass[0]);
    serialPrintInt(pass[1]);
    serialPrintInt(pass[2]);
    serialPrintInt(pass[3]);
    serialPrintln(" ");
}

//Function to send one pair of bits (bit1, bit2) through DATA_PIN1 and DATA_PIN2
void send_bits(){
    P1OUT = alpha;
    __delay_cycles(1000);    // small setup time (~1 ms at 1 MHz)
    // Clock pulse: low -> high -> low
    P1OUT |= CLK_PIN;        // CLOCK = 1  (rising edge)
    __delay_cycles(1000);    // keep high for a moment
    P1OUT &= ~CLK_PIN;       // CLOCK = 0
}

//This will send the chosen sequence of shuffled numbers initially to the other board
void send_sequence(){
    for (counter = 0; counter < 4; counter++) { //00
        if (pass[counter] == 1){
            alpha &= ~(DATA_PIN2 | DATA_PIN1);
        }else if (pass[counter] == 2){//01
            alpha &= ~(DATA_PIN2);
            alpha |= DATA_PIN1;
        }else if (pass[counter] == 3){ //10
            alpha |= DATA_PIN2;
            alpha &= ~DATA_PIN1;
        }else if (pass[counter] == 4){//11
            alpha |= (DATA_PIN2 | DATA_PIN1);
        }
        send_bits();
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

    // P1.0 = CLOCK, P1.3 = DATA as output, start low
    P1DIR |=  (CLK_PIN | DATA_PIN1);
    P1OUT &= ~(CLK_PIN | DATA_PIN1);
    P1DIR &=  ~DATA_PIN2; //input at first

    //-------- PB5 as inputs ----------
    P1DIR &= ~button5;       //Set up PB5 as input
    P1REN |= button5 ;        // Enable internal resistor
    P1OUT |= button5 ;        // Select pull-up resistor (so default = HIGH)
    // Set up the interrupt for PB5
    P1IE |= button5 ;      // Enable interrupt
    P1IES |= button5 ;     //flag is set with a high-to-low transition initially
    P1IFG &= ~button5 ;    // Clear interrupt flag

    alpha = P1OUT;

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

    while (1) {

        //Wait for the second player to be ready
        while (waiting){
            serialPrintln("player NOT Ready");
            unsigned char bit2 = (P1IN & DATA_PIN2) ? 1 : 0;
            if (bit2 == 1){
                serialPrintln("player Ready");
                waiting = 0;
                restart = 1;
                P1DIR |= DATA_PIN2; //set to output
                P1OUT |= DATA_PIN1; //default high
                __delay_cycles(100000);  // wait ~0.1 s before starting the game
                P1OUT &= ~(DATA_PIN1 | DATA_PIN2); //reset data lines
            }
            __delay_cycles(100000);  // wait ~0.1 s between polls
        }

        //Start the game with the logistics
        if (restart == 1){
            P1OUT &= ~(DATA_PIN1 | DATA_PIN2 | CLK_PIN); //reset data lines
            restart = 0;
            shuffle4();
            send_sequence();
            __delay_cycles(500000);  // wait ~1 s before starting the game
            __delay_cycles(500000);
            playing = 1;
        }

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
}

