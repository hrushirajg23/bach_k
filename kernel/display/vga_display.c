/**
 * @file  vga_display.c
 * @brief Functionality for setting up vga buffer
 *         Includes functions to write on the screen.
 *         from OSDev
 * @date 2025-07-14
 */

#include "vga_display.h"
#include <stddef.h>
#include <stdint.h>
#include "string.h"

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer = (uint16_t *)VGA_MEMORY;

// Function to create a VGA entry color by combining foreground and background
// colors
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
  return fg | bg << 4;
}

// Function to create a VGA entry by combining a character and a color
uint16_t vga_entry(unsigned char uc, uint8_t color) {
  return (uint16_t)uc | (uint16_t)color << 8;
}

// Function to initialize the terminal
void terminal_initialize(void) {
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal_buffer = (uint16_t *)VGA_MEMORY;

  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
  }
}

// Function to set the terminal color
void terminal_setcolor(uint8_t color) { terminal_color = color; }

// Function to write a character to a specific position on the terminal
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
}

// Function to write a character to the current position on the terminal
void terminal_putchar(char c) {
  if (c == '\n') {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT)
      terminal_row = 0;
  } else if (c == '\b') {
    if (terminal_column > 0) {
      terminal_column--;
      terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    } else if (terminal_row > 0) {
      terminal_row--;
      terminal_column = VGA_WIDTH - 1;
      terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    }
  } else {
    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
      terminal_column = 0;
      if (++terminal_row == VGA_HEIGHT)
        terminal_row = 0;
    }
  }
}

// Function to write a string on screen
void terminal_write(const char *data, size_t size) {
  for (size_t i = 0; i < size; i++)
    terminal_putchar(data[i]);
}

void terminal_writestring(const char *data) {
  terminal_write(data, strlen(data));
}

/* size_t strlen(const char *str) { */
/*   size_t len = 0; */
/*   while (str[len]) */
/*     len++; */
/*   return len; */
/* } */
