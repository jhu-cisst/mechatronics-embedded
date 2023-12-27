# Mechatronics Embedded

This repository contains the embedded software for the Zynq 7000 SoC used in the
[FPGA1394 V3](https://github.com/jhu-cisst/FPGA1394) board.
The build process is implemented using CMake to invoke the Xilinx build tools, including
Vivado, Vitis and (on Linux only) Petalinux.
It has been tested with Vivado/Vitis 2022.2 and 2023.1 on Windows 10 (with Visual Studio 2017) and
with Vivado/Vitis/Petalinux 2022.2 and 2023.1 on Ubuntu 20.04 (see below).

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

## Build Process

1. Use Vivado to create the Xilinx Support Archive (XSA) file, which describes the hardware design,
from the block design. The source files are located in the `block_design` sub-directory.
See the file UseVivado.cmake for further information. If the `USE_VIVADO` CMake option is `OFF`,
it is necessary to manually specify the path to a programmer-supplied XSA file, which is then
used in the next build steps.

2. Use Vitis, via its command-line tool XSCT, to build the embedded (C/C++) software.
There are two supported platforms (and domains):

   1. Standalone (`platform_standalone`): This platform/domain is for applications that do not use an operating system (i.e., "bare metal" applications) and it includes the ability to  use the Vitis bootgen tool to create the BOOT.bin file that can be written to the MicroSD card. This is mainly useful for low-level testing.

   2. Linux (`platform_linux`): This platform/domain is for applications that run on the Petalinux system. Although it is possible to add Linux applications to the Petalinux build (see below), it is usually more convenient to develop them here. It is recommended to use the petalinux-generated sysroot (see `fpgav3-sysroot-cortexa9.zip` below) rather than the default sysroot.

3. Use Petalinux to build a Linux image and package it for deployment (e.g., via the MicroSD card). It is possible to add applications to the Linux image, though in most cases it would be more convenient to build them with Vitis, using the Linux platform/domain described  above, or to cross-compile them as documented in the `cross_compile` subdirectory. The source files are in the `petalinux` sub-directory.

## Output Files

The Release files consist of two ZIP files in the petalinux/fpgav3-generated directory:

  * `fpgav3-micro-sd.zip` -- Contains all files to be copied to MicroSD card (see below)
  * `fpgav3-sysroot-cortexa9.zip` -- Sysroot for cross-compiling Linux applications for Zynq ARM processor

The `fpgav3-micro-sd.zip` file contains the following files, which are also available in the petalinux/SD_Image directory in the build tree:

  * `BOOT.bin` -- first stage boot loader
  * `boot.scr` -- U-boot script file
  * `image.ub` -- Linux kernel image
  * `FPGA1394V3-QLA.bit` -- firmware for QLA board
  * `FPGA1394V3-DQLA.bit` -- firmware for DQLA board
  * `FPGA1394V3-DRAC.bit` -- firmware for dRAC board
  * `qspi-boot.bin` -- standalone first stage boot loader to copy to QSPI flash
  * `espm.xsvf` -- firmware for ESPM in dVRK-Si arm (PSM or ECM)
  * `version.txt` -- text file containing version information
  * `ReadMe.txt` -- text file describing all files

Note that the `fpgav3init` application compiled with the Linux kernel will autorun at startup, detect the connected board and then load the appropriate firmware (`bit` file). It will also copy `qspi-boot.bin` to the first partition in the flash, if not already there.

## Deploying to MicroSD card

The contents of fpgav3-micro-sd.zip should be extracted to a FAT32 partition on the MicroSD card used with FPGA V3.
Note that for MicroSD cards greater than 32 GB, it may be difficult to format as FAT32; for example, Windows
only provides the option to format as exFAT or NTFS.

## Release Notes

  * Rev 1.0.1 (November 27, 2023)
    * Create toolchain files to cross-compile with Vitis or clang
    * Support use of sysroot from GitHub release
    * Added EMIO_GetVerbose and EMIO_SetVerbose methods
    * Implemented real-time block read and write of FPGA registers via EMIO
    * ESPM Firmware Version 2, which improves reliability of reading instrument id

  * Rev 1.0.0 (August 10, 2023)
    * Initial Release
    * Uses Firmware Rev 8
    * Output files built with Xilinx 2023.1 tools on Ubuntu 20.04

## Xilinx Tool Version Dependencies

Following are the dependencies on the Xilinx tool versions (2022.2, 2023.1):

* **block_design (Vivado)**: the exported TCL file, `exported-block-v31.tcl`, contains the Vivado version string ("2022.2"). This is the only substantive
difference between the TCL files exported by 2022.2 and 2023.1, so the current solution is to replace the version string while copying the file from
the source tree to the build tree. A different solution may be necessary if there are more substantive changes in future Vivado versions.

* **platform_standalone (Vitis)**: the light-weight IP (lwip) library versions are different (lwip211 for 2022.x and lwip213 for 2023.1). Furthermore,
both library versions require a patch to the file `xemacpsif_physpeed.c` to handle the RTL8211F PHY. Other Vitis versions will attempt to use the
2023.1 files, which may fail (e.g., if the lwip version is different).

* **platform_linux (Vitis)**: no differences.

* **petalinux (Petalinux)**: the `config` and `rootfs_config` files are version-specific and are located in 2022.2 and 2023.1 subdirectories of
the `configs` subdirectory. Other Petalinux versions will use the 2023.1 files, with the expectation that a new subdirectory will subsequently be
created. The `bsp.cfg` file is currently not version-specific.

## Building on Ubuntu 20.04

It is necessary to install `libtinfo5`, e.g., `sudo apt install libtinfo5` in addition
to the usual development tools.

Vitis 2022.2 and 2023.1 already contain CMake V3.3.2, which is used if the Xilinx
`settings64.sh` file is sourced before calling CMake. Note that it is not necessary to
source `settings64.sh`, as long as the paths to Vivado and Vitis (xcst) are specified via
the CMake GUI.

Petalinux 2022.2 and 2023.1 specify that the following packages should be installed (`apt-get install`):
iproute2 gawk python3 python build-essential gcc git make net-tools libncurses5-dev tftpd zlib1g-dev libssl-dev flex bison libselinux1 gnupg wget git-core diffstat chrpath socat xterm autoconf libtool tar unzip texinfo zlib1g-dev gcc-multilib automake zlib1g:i386 screen pax gzip cpio python3-pip python3-pexpect xz-utils debianutils iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint3

## Building on Windows

Building on Windows is not officially supported, although both Vivado and Vitis are available and thus it should be possible to build everything except Petalinux.

We have not had success, however, running Vitis from Visual Studio, even when disabling parallel builds (e.g., in Visual Studio, Tools...Options...Projects and Solutions...Build and Run...maximum number of parallel project == 1).

In addition, although the cross-compile build subdirectories (`cc_vitis` and `cc_clang`) are created, they currently are not functional.

## Building on OS X

None of the Xilinx tools are available on Mac OS X and therefore it is only possible to cross-compile. In particular, `clang` is well supported on OS X and can be used to cross-compile the custom fpgav3 library and apps used with Petalinux. See the `cross_compile` subdirectory for more information.
