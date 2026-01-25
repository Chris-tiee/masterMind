/*
 * Board A: Sender
 */

// Include the given template
#include <templateEMP.h>

//Communication pins
#define CLK_PIN   BIT0      // P1.0
#define DATA_PIN1  BIT3      // P1.3
#define DATA_PIN2  BIT4      // P1.4

#define LED_STATUS   BIT0      // LED on P3.0  
#define LED_PLAYER1  BIT1      // LED on P3.1 
#define LED_PLAYER2  BIT2      // LED on P3.2  


//Shift Register Pins
#define clr BIT5 //P2.5
#define s01 BIT6 //P2.6 
#define s02 BIT0 //P2.0
#define s12 BIT1 //P2.1
#define s01 BIT2 //P2.2
#define s11 BIT3 //P2.3
#define ck BIT4  //P2.4
#define sr2 BIT6 //P2.6
#define QD1 BIT7 //P2.7

void pulseCK(void){
    P2OUT |= ck;
   __delay_cycles(5);
    P2OUT &= ~ck;
}

volatile int pass[4] = {1,2,3,4}; //to hold the sequence/
//The secret sequence will be a permutation of these four numbers
//These numbers represent the push buttons PB1/PB2/PB3/PB4 respectively

unsigned char alpha = 0; //to set the state of the data pins sent
volatile int counter = 0; //to loop over the sequence

volatile int waiting = 1; //to indicate when we are waiting for the players to be ready

volatile int  restart = 1; //to  indicate when we want to restart the game
volatile int  playing = 0; //to indicate if the game is ongoing

volatile int guess[4] = {0,0,0,0};  //to hold the players guesses
volatile int guess_counter = 0;     //to wait for 4 button presses

volatile int won1 = 0; //to indicate if player 1 has won or not
volatile int won2 = 0; //to indicate if player 2 has won or not

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

void send_bits(){
    P1OUT = alpha;
    __delay_cycles(1000);    // small setup time (~1 ms at 1 MHz)

    // Clock pulse: low -> high -> low
    P1OUT |= CLK_PIN;        // CLOCK = 1  (rising edge)
    __delay_cycles(1000);    // keep high for a moment
    P1OUT &= ~CLK_PIN;       // CLOCK = 0
}

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
        }else if (pass[counter == 4]){//11
            alpha |= (DATA_PIN2 | DATA_PIN1);
        }
        send_bits();
    }
}

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
//Flashing if waiting for players to be ready
//On if player can intput the sequence
#pragma vector = TIMER1_A0_VECTOR
__interrupt void Timer1_A0_ISR(void){
    P3OUT ^= LED_STATUS;
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

    //Set up interrupt for status LED every 0.5s
    P3DIR |= (LED_STATUS | LED_PLAYER1 | LED_PLAYER2);
    P3OUT &= ~(LED_STATUS | LED_PLAYER1 | LED_PLAYER2);
    TA1CCR0  = 62500 - 1;
    TA1CCTL0 = CCIE;        // enable CCR0 interrupt
    TA1CTL   = TASSEL_2 |ID_3| MC_1 | TACLR; // SMCLK, up mode, clear

    while (1) {

        //Wait for the second player to be ready
        while (waiting){
            serialPrintln("player NOT Ready");
            // Clock pulse: low -> high -> low
            P1OUT |= CLK_PIN;        // CLOCK = 1  (rising edge)
            __delay_cycles(1000);    // keep high for a moment
            P1OUT &= ~CLK_PIN;       // CLOCK = 0
            // sample DATA on rising edge
            unsigned char bit2 = (P1IN & DATA_PIN2) ? 1 : 0;
            if (bit2 == 1){
                serialPrintln("player Ready");
                waiting = 0;
                // restart = 1;
                P1DIR |= DATA_PIN2;
                P1OUT |= DATA_PIN1; //default high
                P1OUT |= CLK_PIN;        // CLOCK = 1  (rising edge)
                __delay_cycles(1000);    // keep high for a moment
                P1OUT &= ~CLK_PIN;       // CLOCK = 0
            }
            __delay_cycles(100000);  // wait ~0.1 s before starting the game
            P1OUT &= ~(DATA_PIN1 | DATA_PIN2); //reset data lines
        }

        //Start the game with the logistics
        if (restart == 1){
            restart = 0;
            shuffle4();
            send_sequence();
            __delay_cycles(500000);  // wait ~1 s before starting the game
            __delay_cycles(500000);
            playing = 1;
        }
        
        while (restart == 0){
            if (playing == 1){
                serialPrintln("Input guess");
                //Stop timer of status LED and turn it on
                // This indicates that player 1 is ready to play
                TA0CTL = 0;        // stop timer
                TA0CCTL0 &= ~CCIFG;
                P3OUT |= LED_STATUS; //turn on status LED

                guess_counter = 0;
                guess[4] = {0,0,0,0};
                
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
                int won1 = 1;
                for (int i = 0; i <4; i++){
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
                // Clock pulse: low -> high -> low
                P1OUT |= CLK_PIN;        // CLOCK = 1  (rising edge)
                __delay_cycles(1000);    // keep high for a moment
                P1OUT &= ~CLK_PIN;       // CLOCK = 0
                //We will wait for the other player's result
                unsigned char bit1 = (P1IN & DATA_PIN1) ? 1 : 0;
                unsigned char bit2 = (P1IN & DATA_PIN2) ? 1 : 0;
                if (bit2 && bit1){
                    won2 = 1;
                }else if (!bit2 && bit1){
                    won2 = 0;
                }
                serialPrintln("Other player's result received");

                //Check the results
                if (bit1){
                    if (won1 && won2){
                        serialPrintln("Both players WON!");
                        P3OUT |= (LED_PLAYER1 | LED_PLAYER2);
                        restart = 1; //restart the game
                        for (int i = 0; i<10; i++){
                            __delay_cycles(500000);
                        }
                    }else if (won1 && !won2){
                        serialPrintln("You WON! Other player LOST!");
                        P3OUT |= LED_PLAYER1;
                        restart = 1; //restart the game
                        for (int i = 0; i<10; i++){
                            __delay_cycles(500000);
                        }
                    }else if (!won1 && won2){
                        serialPrintln("You LOST! Other player WON!");  
                        P3OUT |= LED_PLAYER2;
                        restart = 1; //restart the game
                        for (int i = 0; i<10; i++){
                            __delay_cycles(500000);
                        }
                    }else{
                        serialPrintln("Both players LOST!");
                        playing = 1; //restart playing
                    }
                }

            }
        }

    }
}

