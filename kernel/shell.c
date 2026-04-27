#include "shell.h"
#include "serial.h"
#include "string.h"
#include "fs.h"

#define CMD_MAX_LEN 256
static char cmd_buf[CMD_MAX_LEN];
static int cmd_len = 0;

/* skip leading spaces, return pointer into buf */
static char *skip_spaces(char *s) {
    while (*s == ' ') s++;
    return s;
}

/* ---- command handlers ---- */

static void cmd_help(void) {
    printk("\nAvailable commands:\n");
    printk("  help          - Show this message\n");
    printk("  echo [msg]    - Print message\n");
    printk("  clear         - Clear screen\n");
    printk("  ls            - List root directory\n");
    printk("  cat <file>    - Print file contents\n");
    printk("  uname         - Print OS name\n");
}

static void cmd_ls(void) {
    printk("\n");
    ls_root();
}

static void cmd_cat(char *args) {
    char *fname = skip_spaces(args);
    if (*fname == '\0') {
        printk("\nUsage: cat <filename>\n");
        return;
    }
    printk("\n");
    cat_file(fname);
}

static void cmd_echo(char *args) {
    printk("\n");
    if (*args == ' ') {
        printk("%s", args + 1);
    }
    printk("\n");
}

static void cmd_uname(void) {
    printk("\nBach Kernel v0.1 (i386)\n");
}

/* ---- main dispatch ---- */

void shell_execute(char *cmd) {
    if (cmd[0] == '\0')
        return;

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        printk("\n");
        /* nothing to clear on serial, just print a bunch of newlines */
        for (int i = 0; i < 40; i++) printk("\n");
    } else if (strncmp(cmd, "echo", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        cmd_echo(cmd + 4);
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls();
    } else if (strncmp(cmd, "cat", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) {
        cmd_cat(cmd + 3);
    } else if (strcmp(cmd, "uname") == 0) {
        cmd_uname();
    } else {
        printk("\nUnknown command: %s\n", cmd);
    }
}

void shell_init(void) {
    printk("\n> ");
}

void shell_handle_char(char c) {
    if (c == 0) return;

    if (c == '\n') {
        cmd_buf[cmd_len] = '\0';
        shell_execute(cmd_buf);
        cmd_len = 0;
        printk("\n> ");
    } else if (c == '\b') {
        if (cmd_len > 0) {
            cmd_len--;
            /* erase last char on serial terminal: BS, space, BS */
            printk("\b \b");
        }
    } else {
        if (cmd_len < CMD_MAX_LEN - 1) {
            cmd_buf[cmd_len++] = c;
            serial_writechar(c);
        }
    }
}
