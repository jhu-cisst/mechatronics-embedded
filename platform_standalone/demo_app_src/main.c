/*
 * Demo application
 */


#include <stdio.h>
#include "platform.h"

int main()
{
    init_platform();

    printf("Demo application, based on Xilinx Hello World template\n\r");

    cleanup_platform();
    return 0;
}
