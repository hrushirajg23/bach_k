/*
 *  The PS/2 Controller itself uses 2 IO ports (IO ports 0x60 and 0x64). 
    IO Port 	Access Type 	Purpose
    [ 0x60 port is for the keyboard encoder i.e the microcontroller inside keyboard]

    0x60 	Read/Write 	        read input buffer
    0x60       write            send command

    [ 0x64 port is for the onboard(motherboard)'s keyboard microcontroller]
    0x64 	Read 	            Status Register 
    0x64 	Write 	            Command Register

    status register 0x64

    bit 0: output buffer status (0 = empty, 1 = full)
    (must be set before attempting to read data from IO port 0x60

    bit 1: input buffer status (0 = empty, 1= full)
    (must be clear before attempting to write data to IO port 0x60 or IO port 0x64)

    bit 2: system flag

    bit 3: command data
    (0 = data written to input buffer is data for PS/2 device, 
    1 = data written to input buffer is data for PS/2 controller command) 

    bit 4: unknown
    bit 5: unknown
    bit 6: timeout error
    bit 7: parity error

    Sending a command to 0x60:
        Before doing this however, you need to insure that bit 0 
        (output buffer full) of the keyboard controller status register 
        is 0 to insure it is safe. If bit 1 (input buffer full) of the 
        keyboard controller status register is 1 then data is in the input 
        buffer ready to be read.

    Writing a value to port 0x64 will allow you to send a command byte to the onboard keyboard controller. Reading from port 0x64 will allow you to get the status byte of the keyboard controller
*/
#include <stdint.h>
#include <stdbool.h>
#include "serial.h"
#include "io_access.h"
#include "keyboard.h"
#include "pic.h"
#include "exceptions.h"
#include "shell.h"
#include "tty.h"

extern void SAVE_ALL();
extern int RESTORE_ALL();

unsigned char key_map[128] = {
    0,   27,  // 0x00, 0x01 (ESC)
    '1','2','3','4','5','6','7','8','9','0', // 0x02-0x0B
    '-','=',  // 0x0C-0x0D
    '\b',     // 0x0E Backspace
    '\t',     // 0x0F Tab

    'q','w','e','r','t','y','u','i','o','p', // 0x10-0x19
    '[',']', // 0x1A-0x1B
    '\n',    // 0x1C Enter
    0,       // 0x1D Ctrl
    'a','s','d','f','g','h','j','k','l',     // 0x1E-0x26
    ';','\'','`', // 0x27-0x29
    0,       // 0x2A Left Shift
    '\\',    // 0x2B
    'z','x','c','v','b','n','m',             // 0x2C-0x32
    ',', '.', '/', // 0x33-0x35
    0,       // 0x36 Right Shift
    '*',     // 0x37 Keypad *
    0,       // 0x38 Alt
    ' ',     // 0x39 Space

    // 0x3A - 0x44 (CapsLock + F1-F10)
    0,0,0,0,0,0,0,0,0,0,0,

    // 0x45 - 0x53 (NumLock, keypad etc.)
    0,0,0,0,0,'-',0,0,0,'+',0,0,0,0,

    // Rest unused
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

unsigned char shift_map[128] = {
    0,   27,
    '!','@','#','$','%','^','&','*','(',')', // 0x02-0x0B
    '_','+',  // 0x0C-0x0D
    '\b',
    '\t',

    'Q','W','E','R','T','Y','U','I','O','P', // 0x10
    '{','}',
    '\n',
    0,
    'A','S','D','F','G','H','J','K','L',
    ':','"','~',
    0,
    '|',
    'Z','X','C','V','B','N','M',
    '<','>','?',
    0,
    '*',
    0,
    ' ',

    0,0,0,0,0,0,0,0,0,0,0,

    0,0,0,0,0,'-',0,0,0,'+',0,0,0,0,

    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

unsigned char alt_map[128] = {
    0, 0,
    0,'@',0,'$',0,0,'{','[',']','}',0,0, // numbers row
    0,0,

    0,0,0,0,0,0,0,0,0,0,
    0,0,
    '\n',
    0,

    0,0,0,0,0,0,0,0,0,
    0,0,0,
    0,
    '\\',
    0,0,0,0,0,0,0,
    0,0,0,
    0,
    '*',
    0,
    ' ',

    0,0,0,0,0,0,0,0,0,0,0,

    0,0,0,0,0,0,0,0,0,0,0,0,0,0,

    0,0,0,0,0,0,0,0
};

void wait_input_empty() {
    while (inb(0x64) & KYBRD_CTRL_STATS_MASK_IN_BUF);   // till input buffer full wait
}

void wait_output_full() {
    while (!(inb(0x64) & KYBRD_CTRL_STATS_MASK_OUT_BUF)); // till output buffer empty wait
}

uint8_t kb_ctrl_rd_status() 
{
    return inb(KYBRD_CTRL_STATS_REG);
};

void kb_ctrl_send_cmd(uint8_t cmd)
{
    //! wait for kkybrd controller input buffer to be clear
    wait_input_empty();
    outb(KYBRD_CTRL_CMD_REG, cmd);
}

uint8_t kb_enc_read_buf () {
 
	return inb(KYBRD_ENC_INPUT_BUF);
}

//! send command byte to keyboard encoder
void kybrd_enc_send_cmd (uint8_t cmd) {
 
    wait_input_empty();
	outb(KYBRD_ENC_CMD_REG, cmd);
}
/*
 *
 *
| Port   | Direction | Purpose                                           |
| ------ | --------- | ------------------------------------------------- |
| `0x64` | write     | **command register** (tell controller what to do) |
| `0x64` | read      | **status register**                               |
| `0x60` | write     | **data input** (send data to controller/keyboard) |
| `0x60` | read      | **data output** (read result / scan code)         |

basic idea:

CPU → [0x64] → "do this command"
CPU → [0x60] → "here is data for that command"

Controller → [0x60] → "here is your result"
*/

void keyboard_driver(void)
{
    /* SAVE_ALL(); */
    /* printk("inside keyboard driver\n"); */
    static bool e0 = false;

    uint8_t scancode = inb(0x60);

    // ignore key release
    if (scancode & 0x80) {
        send_EOI(1);
        return;
    }

    char c = key_map[scancode];
    if (c) {
        do_tty(c);
    }

    send_EOI(1);

    /* RESTORE_ALL(); */
}
void kb_init(void) 
{
    uint8_t cmd;

    asm volatile ("cli");

    /* ---- 1. Flush any stale bytes from the PS/2 output buffer ---- */
    while (inb(0x64) & KYBRD_CTRL_STATS_MASK_OUT_BUF)
        inb(0x60);

    /* ---- 2. Read the Controller Configuration Byte (CCB) ---- */
    wait_input_empty();
    outb(0x64, 0x20);          

    wait_output_full(); //wait joparyant output full nahi hoat
    cmd = inb(0x60);

    cmd |= 0x01;    // enable keyboard interrupt (IRQ1)
    cmd &= ~0x10;   // enable keyboard clock (clear disable bit)
    cmd |= 0x40;    // enable translation (Set2 to  Set1)

    /* ---- 3. Write back the modified CCB ---- */
    wait_input_empty();
    outb(0x64, 0x60);

    wait_input_empty();
    outb(0x60, cmd);

    /* ---- 4. Enable keyboard port ---- */
    wait_input_empty();
    outb(0x64, 0xAE);

    /* ---- 5. Tell the keyboard to start scanning (0xF4) ---- */
    wait_input_empty();
    outb(0x60, 0xF4);          // Enable Scanning

    /* Wait for the keyboard's ACK (0xFA) and discard it */
    wait_output_full();
    inb(0x60);                 // read & discard ACK

    /* Flush anything else the controller might have queued */
    while (inb(0x64) & KYBRD_CTRL_STATS_MASK_OUT_BUF)
        inb(0x60);

    asm volatile ("sti");
    printk("[kb_init] keyboard initialized\n");
}

