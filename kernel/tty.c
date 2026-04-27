#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "serial.h"
#include "keyboard.h"
#include "shell.h"

void do_tty(uint8_t key)
{
    shell_handle_char((char)key);
}
