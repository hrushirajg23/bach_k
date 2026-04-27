#ifndef TTY_H
#define TTY_H

#define CB_SIZE 64

struct clist {
    int head;
    int tail; 
    char c_data[CB_SIZE];
};
void do_tty(uint8_t key);

#endif
