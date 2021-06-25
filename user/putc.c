#include "stdlib.h"


int main() {
    print("Hello world!\n");
    //sys_yield();
    //print("Hello world again!\n");
    sys_exit(0);
    print("putc: Should never be here!");
    while (1);
}