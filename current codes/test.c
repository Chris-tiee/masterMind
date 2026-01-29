// Haven't added this yet 



// //The frequency of the melodies stored in a list
// volatile int loserMelody[6] = {550, 600, 700, 800, 900, 1000};
// volatile int gameStartMelody[6] = {800, 600, 740, 800, 600, 650};

// Lower numbers = higher pitch (TA0CCR0 period counts)

// Sad / losing melody (descending, clearly “game over”)
volatile int loserMelody[6] = {1517,1703,1912,2273,2551,3817};
// Game start melody (bright, arcade-style)
volatile int gameStartMelody[6] = {1912, 1517, 1276,  956, 1276, 956};

// Game start melody (bright, arcade-style)
volatile int gameStartMelody[6] = {
    1912,  // C5
    1517,  // E5
    1276,  // G5
     956,  // C6
    1276,  // G5
     956   // C6
};


//Function that plays a specific note
void playNote(frequency){
    TA0CCR0 = frequency; //Frequency   // PWM Period
    TA0CCR2 = 500; // CCR2 PWM duty cycle (50 %)
    __delay_cycles(500000); //500ms
}


//Initialize piezzo as output buzzer
P3DIR |= BIT6;  // P3.6 output
P3REN &= ~BIT6; //disable internal resistor
P3SEL |= BIT6;  // P3.6 TA0.2 option
TA0CCTL2 = OUTMOD_3; // CCR2 set/reset
//Starting with no sound initially
TA0CCR0 = 0; //Frequency   // PWM Period: 1000 us
//Higher frequency means lower number (1000=1kHz, 100=10kHz, 500 =2kHz)
TA0CCR2 = 0; // CCR2 PWM duty cycle (50 %)
TA0CTL = TASSEL_2 + MC_1; // SMCLK; MC_1-> up mode;

int melody_counter = 0;
for (melody_counter = 0; melody_counter < 6; melody_counter++) {
        playNote(loserMelody[melody_counter]);
}
melody_counter = 0;

//shut down the sound
TA0CCR0 = 0; //Frequency   // PWM Period
TA0CCR2 = 0; // CCR2 PWM duty cycle (50 %)
__delay_cycles(50000); //50ms to allow pin to reach high level
