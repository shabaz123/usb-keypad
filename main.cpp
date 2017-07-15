/*************************************
 * USB Keyboard
 * rev 1.0 shabaz July 2017
 *
 * license: free for all non-commercial use
 *
 *************************************/
 
/*************************************
 * includes
 *************************************/
#include "mbed.h"
#include "USBKeyboard.h"

/*************************************
 * defines
 *************************************/
#define COL0 PTC11
#define COL1 PTC10
#define COL2 PTC6
#define COL3 PTC5

#define ROW0 PTC4
#define ROW1 PTC3
#define ROW2 PTC0
#define ROW3 PTC7

// timings/counts to get good keypress behavior
#define TICK_TIME 0.005
#define DEBOUNCE_NUM 2
#define FIRST_WAIT_NUM 100
#define NEXT_WAIT_NUM 1

// states to handle the keypress behaviour
#define BSTATE_IDLE 0
#define BSTATE_FIRST 1
#define BSTATE_FIRST_WAIT 2
#define BSTATE_NEXT_WAIT 3

/*************************************
 * constants
 *************************************/
const char chardefault[4][4]={  {'1', '2', '3', 'a'},
                                {'4', '5', '6', 'B'},
                                {'7', '8', '9', 'C'},
                                {'*', '0', '#', 'D'}};

// valid values for modifier: 0, KEY_CTRL, KEY_SHIFT, KEY_ALT
const char modifierdefault[4][4]={  {0, 0, 0, KEY_CTRL},
                                    {0, 0, 0, 0},
                                    {0, 0, 0, 0},
                                    {0, 0, 0, 0}};

                                
/*************************************
 * global variables
 *************************************/
BusOut leds(LED1, LED2, LED3);
DigitalOut* row[4];
DigitalIn* col[4];
char charmap[4][4];
char modifiermap[4][4];
char tstring[2];
char isattached;
char dosend;
char keypad_event;
char keyval_store;
char modifier_store;
char bstate;
unsigned int tick_count;

DigitalOut row0(ROW0);
DigitalOut row1(ROW1);
DigitalOut row2(ROW2);
DigitalOut row3(ROW3);

DigitalIn col0(COL0);
DigitalIn col1(COL1);
DigitalIn col2(COL2);
DigitalIn col3(COL3);

USBKeyboard kb;

Ticker press_ticker;

/*************************************
 * functions
 *************************************/

// copy from ROM into RAM the default config
void
init_map(void)
{
     memcpy(charmap, chardefault, 16);
     memcpy(modifiermap, modifierdefault, 16);
}

// scan the entire keypad and then map any pressed button
// into the kepyress value and any modifier
char
kbscan(char* modifier)
{
    char ret=0;
    char row_idx=0;
    char col_idx=0;
    
    for (row_idx=0; row_idx<4; row_idx++)
    {
        row[row_idx]->write(0);
        for (col_idx=0; col_idx<4; col_idx++)
        {
            if (col[col_idx]->read()==0)
            {
                // a button is pressed
                ret=charmap[row_idx][col_idx];
                *modifier=modifiermap[row_idx][col_idx];
            }
        }
        row[row_idx]->write(1);
    }
    return(ret);
}

// send the keypress (with any modifier) to the PC
void send_kb(char keyval, char modifier)
{
    switch (modifier)
    {
        case 0:
            tstring[0]=keyval;
            kb.printf(tstring);
            break;
        default:
            kb.keyCode(keyval, modifier);
            break;
    }
}

// keypress handler engine. This kicks in whenever a button is pressed
// and handles debounce and key repeat, and runs periodically (timer tick)
// until the key is finally released by the user
void press_handler(void)
{
    char tempkey;
    char modifier;
    tempkey=kbscan(&modifier);
    
    switch (bstate)
    {
        case BSTATE_IDLE:
            // the keypress has been sent
            // now lets wait a short debounce period
            tick_count=0;
            bstate=BSTATE_FIRST;
            break;
        case BSTATE_FIRST:
            if (tempkey!=keyval_store) // key released!
            {
                bstate=BSTATE_IDLE;
                press_ticker.detach();
                isattached=0;
            }
            else if (tick_count>=DEBOUNCE_NUM) // debounce period expired
            {
                tick_count=0;
                bstate=BSTATE_FIRST_WAIT;
            }
            break;
        case BSTATE_FIRST_WAIT:
            if (tempkey!=keyval_store) // key released!
            {
                bstate=BSTATE_IDLE;
                press_ticker.detach();
                isattached=0;
            }
            else if (tick_count>=FIRST_WAIT_NUM) // first key repeat time expired
            {
                // we can send a keypress
                tstring[0]=keyval_store;
                
                tick_count=0;
                bstate=BSTATE_NEXT_WAIT;
                dosend=1;                
            }
            break;
        case BSTATE_NEXT_WAIT:
            if (tempkey!=keyval_store) // key released!
            {
                bstate=BSTATE_IDLE;
                press_ticker.detach();
                isattached=0;
            }
            else if (tick_count>=NEXT_WAIT_NUM) // key repeat time expired
            {
                // we can send a keypress faster now
                tstring[0]=keyval_store;
                
                tick_count=0;
                dosend=1;
                // stay in this current BSTATE
            }
            break;
        default:
            break;
    }
    tick_count++;
}

/******************************************
 * main function
 ******************************************/
int
main(void)
{
    char i;
    char forever=1;
    char modifier;
    isattached=0;
    keypad_event=0;
    tstring[1]='\0';
    tick_count=0;
    dosend=0;
    bstate=BSTATE_IDLE;
    
    init_map();
    
    row[0]=&row0;
    row[1]=&row1;
    row[2]=&row2;
    row[3]=&row3;
    
    col[0]=&col0;
    col[1]=&col1;
    col[2]=&col2;
    col[3]=&col3;
    
    // set matrix output pins all high, and
    // set matrix input pins to have pullups
    for (i=0; i<4; i++)
    {
        row[i]->write(1); // set all rows high by default
        col[i]->mode(PullUp); // set all columns to have pullups
    }
    
    // main loop
    while (forever)
    {
        if (dosend) // keypress handler engine has invoked a key repeat
        {
            send_kb(keyval_store, modifier_store);
            dosend=0;
        }
        if ((isattached==0) && (bstate==BSTATE_IDLE)) // no current keypress
        {
            keypad_event=kbscan(&modifier);
            if (keypad_event!=0) // new keypress!
            {
                isattached=1;
                keyval_store=keypad_event;
                modifier_store=modifier;
                send_kb(keyval_store, modifier_store);
                press_ticker.attach(&press_handler, TICK_TIME); // activate keypress engine
            }
        }
    }
    
    return(0); // warning on this line is ok
}
