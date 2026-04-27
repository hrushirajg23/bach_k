/**
 * @file  keyboard.h
 * @brief Functionality for keyboard driver
 * @date 2025-07-14
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "io_access.h"

/*
 *
 *
 * PS/2 Port Controller configuration byte
 * Commands 0x20 and 0x60 let you read and write the PS/2 Controller Configuration Byte. 
 * This configuration byte has the following format:
 * 
 * bit 0: first ps/2 port interrupt (1 = enabled, 0 = disabled)
 * bit 1: second ps/2 port interrupt (1 = enabled, 0 = disabled)
 * bit 2: system flag (1 = system passed POST, 0 = your OS shouldn't be running)
 * bit 3: should be 0
 * bit 4: first ps/2 port clock (1 = enabled, 0 = disabeld)
 * bit 5: second ps/2 port clock (1 = enabled, 0 = disabeld)
 * bit 6: first ps/2 port translation (1 = enabled, 0 = diabeld)
 * bit 7: must be zero
 *
 *
 *
 * keyboard -> ps/2 cable -> 8042 controller (on motherboard) -> CPU (via ports 0x60 / 0x64)
 */

#define PS2_COMMAND 0x64
#define PS2_STATUS 0x64
#define PS2_DATA 0x60
#define PS2_ENABLE_KB 0xAE
#define PS2_DISABLE_KB 0xAD

#define PS2_SCAN_RELEASE 0x80

#define PS2_RD_CCB 0x20 // read Controller Configuration Byte
#define PS2_WR_CCB 0x60 // write  Controller Configuration Byte
                        
enum KYBRD_ENCODER_IO {
 
	KYBRD_ENC_INPUT_BUF	=	0x60,
	KYBRD_ENC_CMD_REG	=	0x60
};
 
enum KYBRD_CTRL_IO {
 
	KYBRD_CTRL_STATS_REG	=	0x64,
	KYBRD_CTRL_CMD_REG	=	0x64
};

enum KYBRD_CTRL_STATS_MASK {
 
	KYBRD_CTRL_STATS_MASK_OUT_BUF	=	1,		//00000001
	KYBRD_CTRL_STATS_MASK_IN_BUF	=	2,		//00000010
	KYBRD_CTRL_STATS_MASK_SYSTEM	=	4,		//00000100
	KYBRD_CTRL_STATS_MASK_CMD_DATA	=	8,		//00001000
	KYBRD_CTRL_STATS_MASK_LOCKED	=	0x10,		//00010000
	KYBRD_CTRL_STATS_MASK_AUX_BUF	=	0x20,		//00100000
	KYBRD_CTRL_STATS_MASK_TIMEOUT	=	0x40,		//01000000
	KYBRD_CTRL_STATS_MASK_PARITY	=	0x80		//10000000
};

void wait_input_empty();

void wait_output_full();
uint8_t kb_ctrl_rd_status(); 

void kb_ctrl_send_cmd(uint8_t cmd);

uint8_t kb_enc_read_buf () ;

void kybrd_enc_send_cmd (uint8_t cmd);

void keyboard_driver(void);

// static void (*)(void) ps2_scancode_set1[] = {
// 	 none,do_self,do_self,do_self,	/* 00-03 s0 esc 1 2 */
// 	 do_self,do_self,do_self,do_self,	/* 04-07 3 4 5 6 */
// 	 do_self,do_self,do_self,do_self,	/* 08-0B 7 8 9 0 */
// 	 do_self,do_self,do_self,do_self,	/* 0C-0F + ' bs tab */
// 	 do_self,do_self,do_self,do_self,	/* 10-13 q w e r */
// 	 do_self,do_self,do_self,do_self,	/* 14-17 t y u i */
// 	 do_self,do_self,do_self,do_self,	/* 18-1B o p } ^ */
// 	 do_self,ctrl,do_self,do_self,	/* 1C-1F enter ctrl a s */
// 	 do_self,do_self,do_self,do_self,	/* 20-23 d f g h */
// 	 do_self,do_self,do_self,do_self,	/* 24-27 j k l | */
// 	 do_self,do_self,lshift,do_self,	/* 28-2B { para lshift , */
// 	 do_self,do_self,do_self,do_self,	/* 2C-2F z x c v */
// 	 do_self,do_self,do_self,do_self,	/* 30-33 b n m , */
// 	 do_self,minus,rshift,do_self,	/* 34-37 . - rshift * */
// 	 alt,do_self,caps,func,		/* 38-3B alt sp caps f1 */
// 	 func,func,func,func,		/* 3C-3F f2 f3 f4 f5 */
// 	 func,func,func,func,		/* 40-43 f6 f7 f8 f9 */
// 	 func,num,scroll,cursor,		/* 44-47 f10 num scr home */
// 	 cursor,cursor,do_self,cursor,	/* 48-4B up pgup - left */
// 	 cursor,cursor,do_self,cursor,	/* 4C-4F n5 right + end */
// 	 cursor,cursor,cursor,cursor,	/* 50-53 dn pgdn ins del */
// 	 none,none,do_self,func,		/* 54-57 sysreq ? < f11 */
// 	 func,none,none,none,		/* 58-5B f12 ? ? ? */
// 	 none,none,none,none,		/* 5C-5F ? ? ? ? */
// 	 none,none,none,none,		/* 60-63 ? ? ? ? */
// 	 none,none,none,none,		/* 64-67 ? ? ? ? */
// 	 none,none,none,none,		/* 68-6B ? ? ? ? */
// 	 none,none,none,none,		/* 6C-6F ? ? ? ? */
// 	 none,none,none,none,		/* 70-73 ? ? ? ? */
// 	 none,none,none,none,		/* 74-77 ? ? ? ? */
// 	 none,none,none,none,		/* 78-7B ? ? ? ? */
// 	 none,none,none,none,		/* 7C-7F ? ? ? ? */
// 	 none,none,none,none,		/* 80-83 ? br br br */
// 	 none,none,none,none,		/* 84-87 br br br br */
// 	 none,none,none,none,		/* 88-8B br br br br */
// 	 none,none,none,none,		/* 8C-8F br br br br */
// 	 none,none,none,none,		/* 90-93 br br br br */
// 	 none,none,none,none,		/* 94-97 br br br br */
// 	 none,none,none,none,		/* 98-9B br br br br */
// 	 none,unctrl,none,none,		/* 9C-9F br unctrl br br */
// 	 none,none,none,none,		/* A0-A3 br br br br */
// 	 none,none,none,none,		/* A4-A7 br br br br */
// 	 none,none,unlshift,none,		/* A8-AB br br unlshift br */
// 	 none,none,none,none,		/* AC-AF br br br br */
// 	 none,none,none,none,		/* B0-B3 br br br br */
// 	 none,none,unrshift,none,		/* B4-B7 br br unrshift br */
// 	 unalt,none,uncaps,none,		/* B8-BB unalt br uncaps br */
// 	 none,none,none,none,		/* BC-BF br br br br */
// 	 none,none,none,none,		/* C0-C3 br br br br */
// 	 none,none,none,none,		/* C4-C7 br br br br */
// 	 none,none,none,none,		/* C8-CB br br br br */
// 	 none,none,none,none,		/* CC-CF br br br br */
// 	 none,none,none,none,		/* D0-D3 br br br br */
// 	 none,none,none,none,		/* D4-D7 br br br br */
// 	 none,none,none,none,		/* D8-DB br ? ? ? */
// 	 none,none,none,none,		/* DC-DF ? ? ? ? */
// 	 none,none,none,none,		/* E0-E3 e0 e1 ? ? */
// 	 none,none,none,none,		/* E4-E7 ? ? ? ? */
// 	 none,none,none,none,		/* E8-EB ? ? ? ? */
// 	 none,none,none,none,		/* EC-EF ? ? ? ? */
// 	 none,none,none,none,		/* F0-F3 ? ? ? ? */
// 	 none,none,none,none,		/* F4-F7 ? ? ? ? */
// 	 none,none,none,none,		/* F8-FB ? ? ? ? */
// 	 none,none,none,none,		/* FC-FF ? ? ? ? */
// };

/*
 * port 0x64 read kartana ani write kartana tyacha bits cha artha vegla 
 * asto
 *
 * 0x64 Write kartana:
 * bit 7: reserved
 * bit 6: translate scan code set 2 to set 1
 * bit 5: (1 = disable mouse, 0 = enable mouse)
 * bit 4: (1 = disable keyboard, 0 = enable keyboard)
 * bit 3: reserved
 * bit 2: system flag
 * bit 1: (1 = generate intr when controller mouse's data into its output buffer)
 * bit 0: (1 = generate intr when controller keyboard's data into its output buffer)
 *
 * 0x64 Read kartana:
 * On board 8042 microcontroller status i/o port (read only) 0x64 ( status port of keyboard)
    Bit 0: Output Buffer Status
        0: Output buffer empty, dont read yet
        1: Output buffer full, please read me :)
    Bit 1: Input Buffer Status
        0: Input buffer empty, can be written
        1: Input buffer full, dont write yet
    Bit 2: System flag
        0: Set after power on reset
        1: Set after successfull completion of the keyboard controllers self-test (Basic Assurance Test, BAT)
    Bit 3: Command Data
        0: Last write to input buffer was data (via port 0x60)
        1: Last write to input buffer was a command (via port 0x64)
    Bit 4: Keyboard Locked
        0: Locked
        1: Not locked
    Bit 5: Auxiliary Output buffer full
        PS/2 Systems:
            0: Determins if read from port 0x60 is valid If valid, 0=Keyboard data
            1: Mouse data, only if you can read from port 0x60
        AT Systems:
            0: OK flag
            1: Timeout on transmission from keyboard controller to keyboard. This may indicate no keyboard is present.
    Bit 6: Timeout
        0: OK flag
        1: Timeout
        PS/2:
            General Timeout
        AT:
            Timeout on transmission from keyboard to keyboard controller. Possibly parity error (In which case both bits 6 and 7 are set)
    Bit 7: Parity error
        0: OK flag, no error
        1: Parity error with last byte
 *
 *
 *
 * INPUT / OUTPUT Buffer
 *
 * Output buffer(mhanje controller cha output buffer)
 * jo apan read karaycha asto. 8 bit read only register at i/o port 0x60
 * Ithe aplayal scan code kinvha mouse clicks yetat
 * 
 * Input buffer(mhanje controller cha input buffer)
 * ithe apan data write karaycha asto.
 * Ithe konta data pathvaycha ?
 * direct keyboard encoder la je commands pathvayche astil te
 *
 *
 =============WRITE=======================================
 *  Jevha write karaycha asel tevha adhi controller la sangaycha
 *  ki where 0x60 is the write command, not the port no.
 *
 * mhanje apan keyboard controller la command deun sangtoy
 * ki apan command write karnrare
 *  outb(0x64, 0x60) mhanje "I am going to WRITE command byte"
    then 
    outb(0x60, cmd);   

==============READ==========================================
    Jevha read karaycha asel tevha adhi controller la sangaycha
    ki mala read karaycha ahe
     outb(0x64, 0x20) mhanje "I want to READ command byte"
    
     ani mag read karaycha by
     inb(0x60)
===============================================================

*/

void kb_init(void);
void keyboard_intr(void);

#endif
