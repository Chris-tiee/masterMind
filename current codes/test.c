int role = ROLE_INPUT;   // A starts input
int my_win = 0, other_win = 0;

while (1) {
    if (role == ROLE_INPUT) {
        int guess[4];
        collect_guess4(guess);
        my_win = did_win(guess, pass);

        send_result(my_win);
        role = ROLE_WAIT;

    } else { // ROLE_WAIT
        int r;
        do { r = recv_result(); } while (r < 0);
        other_win = r;

        // Decide next round
        if (my_win || other_win) {
            // someone won -> restart game (optionally generate new sequence)
            // for now: restart handshake/sequence or just reshuffle and resend
            // (your choice)
        } else {
            // nobody won -> A goes back to input again, B waits
            role = ROLE_INPUT;
        }
    }
}


#define STROBE  BIT0
#define DATA1   BIT3
#define DATA2   BIT4

void bus_set_tx(void)
{
    P1SEL  &= ~(STROBE|DATA1|DATA2);
    P1SEL2 &= ~(STROBE|DATA1|DATA2);

    //Outputs
    P1DIR |= (STROBE|DATA1|DATA2);
    P1OUT &= ~(STROBE|DATA1|DATA2);
}

void bus_set_rx(void)
{
    P1SEL  &= ~(STROBE|DATA1|DATA2);
    P1SEL2 &= ~(STROBE|DATA1|DATA2);

    //Inputs with pull-downs
    P1DIR &= ~(STROBE|DATA1|DATA2);
    P1REN |=  (STROBE|DATA1|DATA2);
    P1OUT &= ~(STROBE|DATA1|DATA2); // pull-down
}

static inline void wait_strobe_rising(void)
{
    while (P1IN & STROBE) ;
    while (!(P1IN & STROBE)) ;
}

void send_result(int win)
{
    P1SEL  &= ~(STROBE|DATA1|DATA2);
    P1SEL2 &= ~(STROBE|DATA1|DATA2);

    //Outputs
    P1DIR |= (STROBE|DATA1|DATA2);
    P1OUT &= ~(STROBE|DATA1|DATA2);

    // LOSE = 01 (DATA1=1, DATA2=0), WIN = 11
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

int recv_result(void)
{
    //Inputs with pull-downs
    P1DIR &= ~(STROBE|DATA1|DATA2);
    P1REN |=  (STROBE|DATA1|DATA2);
    P1OUT &= ~(STROBE|DATA1|DATA2); // pull-down

    wait_strobe_rising();
    __delay_cycles(500);

    int b1 = (P1IN & DATA1) ? 1 : 0;
    int b2 = (P1IN & DATA2) ? 1 : 0;

    while (P1IN & STROBE) ;

    // only accept messages with DATA1=1 (valid)
    if (!b1) return -1;  // invalid/noise
    return (b2 ? 1 : 0); // b2=1 => WIN, else LOSE
}
