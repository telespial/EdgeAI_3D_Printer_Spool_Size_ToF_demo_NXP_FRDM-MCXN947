#include "board.h"

void BOARD_InitHardware(void);

int main(void)
{
    BOARD_InitHardware();

    for (;;)
    {
        __asm volatile("nop");
    }
}
