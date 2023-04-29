# Mechatronics Embedded

This repository contains the embedded software for the Zynq 7000 SoC used in the
[FPGA1394 V3](https://github.com/jhu-cisst/FPGA1394) board.
The build process is implemented using CMake to invoke the Xilinx build tools, including
Vivado, Vitis and (on Linux only) Petalinux.
It has been tested with Vivado/Vitis 2022.2 on Windows 10 (with Visual Studio 2017) and
with Vivado/Vitis/Petalinux 2022.2 on Ubuntu 20.04 (see below).

The Zynq 7000 SoC contains both a processor (PS) and FGPA (programmable logic, PL).
The code in this repository targets the PS. The PL firmware, which is written in Verilog,
can be found in the [mechatronics-firmware](https://github.com/jhu-cisst/mechatronics-firmware) repository.
Although Xilinx Vivado supports building of the PL firmware, we continue to use the older
Xilinx ISE tool primarily because it enables us to maintain a common code base between FPGA1394 V3
and the previous generations of FPGA1394 boards (V1 and V2), which are based on Spartan 6 and
are therefore not supported by Vivado. In addition, the firmware built by Vivado
currently does not work on the hardware and this issue would need to be solved.

The top-level CMake file (CMakeLists.txt) in this directory contains options `USE_VIVADO`,
`USE_VITIS` and `USE_PETALINUX` to support different workflows. Note that the `USE_PETALINUX`
option is only supported on Linux because the Xilinx Petalinux tool is only available on Linux.

The basic build process is as follows:

1. Use Vivado to create the Xilinx Support Archive (XSA) file, which describes the hardware design,
from the block design. The source files are located in the `block_design` sub-directory.
See the file UseVivado.cmake for further information. If the `USE_VIVADO` CMake option is `OFF`,
it is necessary to manually specify the path to a programmer-supplied XSA file, which is then
used in the next build steps.

2. Use Vitis, via its command-line tool XSCT, to build the embedded (C/C++) software.
There are two supported platforms (and domains):

   1. Standalone (`platform_standalone`): This platform/domain is for applications that do not use an operating system (i.e., "bare metal" applications) and it includes the ability to  use the Vitis bootgen tool to create the BOOT.bin file that can be written to the MicroSD card. This is mainly useful for low-level testing.

   2. Linux (`platform_linux`): This platform/domain is for applications that run on the Petalinux system. Although it is possible to add Linux applications to the Petalinux build (see below), it is usually more convenient to develop them here.

3. Use Petalinux to build a Linux image and package it for deployment (e.g., via the MicroSD card). It is possible to add applications to the Linux image, though  in most  cases it would be more convenient to build them with Vitis, using the Linux platform/domain described  above. The source files are in the `petalinux` sub-directory.

Note that Steps 2 and 3 overlap and could be done in the opposite order.

## Building on Ubuntu 20.04

It is necessary to install `libtinfo5`, e.g., `sudo apt install libtinfo5` in addition
to the usual development tools.

Vitis 2022.2 already contains CMake V3.3.2, which is used if the Xilinx
`settings64.sh` file is sourced before calling CMake. Note that it is not necessary to
source `settings64.sh`, as long as the paths to Vivado and Vitis (xcst) are specified via
the CMake GUI.
