# Standalone Platform

This directory contains the files necessary to create a standalone ("bare metal") platform using Vitis.

The following standalone applications are included:

  * `demo_app` -- a simple demo application, based on the Hello World template
  * `echo_test` -- an application that uses the Light-Weight IP (`lwip`) library to set up an echo server, for testing the Ethernet connections to the Zynq PS
  * `fsbl` -- the standalone first stage boot loader that will be stored in the QSPI flash (e.g., qspi-boot.bin); it displays information about the FPGA on the console
  * `mfg_test` -- manufacturing test software that displays the board id switch, tests the QSPI flash, and tests the DRAM; it displays a menu on the console
