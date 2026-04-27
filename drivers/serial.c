/**
 * @file  serial.c
 * @brief Functionality to write on serial port
 *        from OSDev
 * @date 2025-07-14
 */

#include <stdint.h>
#include <stdarg.h>
#include "io_access.h"
#include "serial.h"
#include "pic.h"

#define COM1 0x3F8

/* This function converts an integer into a character buffer */
static void get_digits(char *buf, int num, int *i) {
  if (num == 0) {
    buf[(*i)++] = '0';
    return;
  }

  int is_negative = 0;
  if (num < 0) {
    is_negative = 1;
    num = -num;
  }

  while (num > 0) {
    buf[(*i)++] = '0' + (num % 10);
    num /= 10;
  }

  if (is_negative) {
    buf[(*i)++] = '-';
  }
}

/* Initializes the serial port with specific configurations */
void serial_init() {
  outb(COM1 + 1, 0x00); // Disable all interrupts
  outb(COM1 + 3, 0x80); // Enable DLAB (set baud rate divisor)
  outb(COM1 + 0, 0x03); // Set divisor to 3 (io byte) 38400 baud
  outb(COM1 + 1, 0x00); // hi byte
  outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
  outb(COM1 + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
  outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
  outb(COM1 + 1, 0x01); // Enable Rx interrupts
}

/* Writes a single character to the serial port */
void serial_writechar(char c) {
  while (!(inb(COM1 + 5) & 0x20))
    ;
  outb(COM1, c);
}

/* Writes a string to the serial port */
void serial_writestring(const char *str) 
{
  while (*str)
    serial_writechar(*str++);
}

int serial_received() {
   return (inb(COM1 + 5) & 0x20);
}

char serial_readchar() 
{
   while (serial_received() == 0);

   return inb(COM1);
}

void serial_handle(void)
{
    char c = serial_readchar();
    printk("serial character is %c\n", c);
    send_EOI(4);
}
/* Writes an integer to the serial port */
void serial_writeint(int num) {
  char buf[12];
  int i = 0;
  get_digits(buf, num, &i);

  while (i--) {
    serial_writechar(buf[i]);
  }
}

/* Writes a hex number to the serial port */
void serial_writehex(uint32_t num) {
  char hex_chars[] = "0123456789ABCDEF";

  serial_writestring("0x");

  for (int i = 28; i >= 0; i -= 4) {
    char c = hex_chars[(num >> i) & 0xF];
    serial_writechar(c);
  }
}

void serial_writedec(int num) {

    char buf[16];
    register int iCnt = 0;
    if(num == 0){
        serial_writestring("0");
        return;
    }

    if ( num < 0 ){
        serial_writestring("-");
        num = -num;
    }

    while ( num > 0 ){
        buf[iCnt++] = '0' + (num % 10);
        num /= 10;
    }
    while ( iCnt-- ){
        serial_writechar(buf[iCnt]);
    }

}

void printk(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format != '%') {
            serial_writechar(*format++);
            continue;
        }

        format++; // skip '%'

        switch (*format) {

        case 'd':
            serial_writedec(va_arg(args, int));
            break;

        case 'u':
            serial_writedec((unsigned int)va_arg(args, unsigned int));
            break;

        case 'x':
            serial_writehex(va_arg(args, unsigned int));
            break;

        case 'p':
            serial_writestring("0x");
            serial_writehex((uint32_t)va_arg(args, void *));
            break;

        case 'c':
            serial_writechar((char)va_arg(args, int));
            break;

        case 's': {
            char *s = va_arg(args, char *);
            if (s)
                serial_writestring(s);
            else
                serial_writestring("(null)");
            break;
        }

        case 'l':   // handles %ld and %lu
            format++;
            if (*format == 'd')
                serial_writedec(va_arg(args, long));
            else if (*format == 'u')
                serial_writedec(va_arg(args, unsigned long));
            else
                serial_writechar('?');
            break;

        case '%':
            serial_writechar('%');
            break;

        default:
            serial_writechar('%');
            serial_writechar(*format);
            break;
        }

        format++;
    }

    va_end(args);
}

