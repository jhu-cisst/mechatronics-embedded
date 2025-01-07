# Mechatronics Embedded

This repository contains the embedded software for the Zynq 7000 SoC used in the
[FPGA1394 V3](https://github.com/jhu-cisst/FPGA1394) board.
The build process is implemented using CMake to invoke the Xilinx build tools, including
Vivado, Vitis and (on Linux only) Petalinux.
It has been tested with Vivado/Vitis 2022.2 and 2023.1 on Windows 10 (with Visual Studio 2017) and
with Vivado/Vitis/Petalinux 2022.2, 2023.1, 2023.2, 2024.1 and 2024.2 on Ubuntu 20.04 or 22.04 (see below).

The Zynq 7000 SoC contains both a processor (PS) and FGPA (programmable logic, PL).
The code in this repository targets the PS. The PL firmware, which is written in Verilog,
can be found in the [mechatronics-firmware](https://github.com/jhu-cisst/mechatronics-firmware) repository.
Although Xilinx Vivado supports building of the PL firmware, we continue to use the older
Xilinx ISE tool primarily because it enables us to maintain a common code base between FPGA1394 V3
and the previous generations of FPGA1394 boards (V1 and V2), which are based on Spartan 6 and
are therefore not supported by Vivado. In addition, the firmware built by Vivado
currently does not work on the hardware and this issue would need to be solved.

The top-level CMake file (CMakeLists.txt) in this directory contains options `TOOLCHAIN_ONLY`,
`USE_VIVADO`, `USE_VITIS` and `USE_PETALINUX` to support different workflows. Note that the `USE_PETALINUX`
option is only supported on Linux because the Xilinx Petalinux tool is only available on Linux.

## Workflows

All workflows, except "Generate toolchain file(s)", require the software to be built from a git working tree
(i.e., created by `git clone`) because they use the `git` command line program to retrieve version information
(i.e., using `git describe`).

Also, the CMake dependency checking is not perfect, so sometimes it is necessary to either start with
a clean build tree, or to manually force certain subprojects to be rebuilt. Many subprojects create a
`cmake.copy` file in the build tree and deleting this file will cause the subproject to be rebuilt.
Similarly, deleting `petalinux/images/linux/image.ub` will cause the kernel image to be rebuilt.

### 1. Generate toolchain file(s) and download sysroot

This is the default setting on non-Linux platforms (Windows and Mac OS X). On Linux systems,
it is only necessary to set the CMake variable `TOOLCHAIN_ONLY` to `ON`.
The toolchain file(s) are generated during CMake configuration.

The toolchain file for clang, toolchain_clang_fpgav3.cmake, is generated on all platforms, even if
clang is not installed. The toolchain file will attempt to find the clang compiler.

The toolchain file for Vitis gcc, toolchain_vitis_fpgav3.cmake, can only be generated on platforms
where Xilinx Vitis is available (Linux and Windows). In this case, it is necessary to find the Vitis
installation in CMake (e.g., by finding the `xsct` executable that is distributed with Vitis).

After configuring and generating in CMake, it is necessary to build (e.g., to call `make` in the build
tree), which downloads the sysroot used by the toolchain files.

### 2. Complete build (Linux only)

This is the default setting on Linux. Specifically, `TOOLCHAIN_ONLY` is `OFF` and
`USE_VIVADO`, `USE_VITIS` and `USE_PETALINUX` are all `ON`. Note that before calling CMake,
it is necessary to set the environment variables for Petalinux, such as by changing to the
Petalinux root directory and typing `. settings.sh`.

### 3. Partial build (mostly standalone programs, Linux or Windows)

For this workflow, `TOOLCHAIN_ONLY` and `USE_PETALINUX` should be `OFF` and `USE_VIVADO` and `USE_VITIS`
should be `ON`. This will build `platform_standalone` and `platform_linux`.
The `platform_standalone` build tree will contain several boot images (BOOT.bin) corresponding
to different standalone applications (e.g., demo_app, mfg_test, echo_test).

If `USE_PETALINUX_SYSROOT` is `ON`, the `platform_linux` build tree will compile libfpgav3.so
and fpgav3init.elf using Vitis and the specified sysroot (`PETALINUX_SYSROOT_EXTERNAL` in CMake).
This is not particularly useful, however, since it is better to cross-compile them using the toolchain file,
as documented [here](cross_compile/ReadMe.md#automated-example).
Also, it does not currently work on Windows.

## Complete Build Process

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

  * See [GitHub Releases](https://github.com/jhu-cisst/mechatronics-embedded/releases)

## Xilinx Tool Version Dependencies

Following are the dependencies on the Xilinx tool versions (2022.2, 2023.x, 2024.x):

* **block_design (Vivado)**: the exported TCL file, `exported-block-v31.tcl`, contains the Vivado version string ("2022.2"). This is the only substantive
difference between the TCL files exported by 2022.2, 2023.x, and 2024.x so the current solution is to replace the version string while copying the file from
the source tree to the build tree. A different solution may be necessary if there are more substantive changes in future Vivado versions.

* **platform_standalone (Vitis)**: no differences (auto-detects correct light-weight IP library version, e.g., "lwip211", "lwip213", "lwip220")

* **platform_linux (Vitis)**: no differences.

* **petalinux (Petalinux)**: no differences

## Building on Ubuntu 20.04 / 22.04

It is necessary to install `libtinfo5`, e.g., `sudo apt install libtinfo5` in addition
to the usual development tools.

Vitis 2022.2, 2023.1 and 2023.2 already contain CMake 3.3.2, which is used if the Xilinx
`settings64.sh` file is sourced before calling CMake. This version of CMake does not meet the
minimum requirement of 3.16, however, so it is best to avoid sourcing `settings64.sh`.
Note that it is not necessary to source `settings64.sh`, as long as the paths to Vivado and
Vitis (xcst) are specified via the CMake GUI.

Petalinux 2022.2, 2023.1 and 2023.2 specify that the following packages should be installed (`apt-get install`):
iproute2 gawk python3 python build-essential gcc git make net-tools libncurses5-dev tftpd zlib1g-dev libssl-dev flex bison libselinux1 gnupg wget git-core diffstat chrpath socat xterm autoconf libtool tar unzip texinfo zlib1g-dev gcc-multilib automake zlib1g:i386 screen pax gzip cpio python3-pip python3-pexpect xz-utils debianutils iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint3

Ubuntu 20.04 and 22.04 use git instead of git-core and pylint instead of pylint3.

Note: Not all of the above packages are really needed.

## Building on Windows

Both Vivado and Vitis are available on Windows and thus it should be possible to build everything except Petalinux.
However, it seems that on Windows, Vitis does not work reliably with parallel builds.
We therefore recommend using a command-line build tool, such as Ninja, that supports disabling parallel builds (i.e., `ninja -j1`).

We have not had success running Vitis from Visual Studio, even when disabling parallel builds (e.g., in Visual Studio, Tools...Options...Projects and Solutions...Build and Run...maximum number of parallel project == 1).
There does not seem to be an option to disable parallel builds in NMake.

Currently, Ninja can build everything except the `platform_linux` library and application; however, the problem appears to be due to Vitis on Windows (at least for 2023.1).

The cross-compile build subdirectories (`cc_vitis` and `cc_clang`) are created and we have successfully cross-compiled `cc_vitis` using Ninja (in this case, `-j1` is not necessary).

## Building on OS X

None of the Xilinx tools are available on Mac OS X and therefore it is only possible to cross-compile. In particular, `clang` is well supported on OS X and can be used to cross-compile the custom fpgav3 library and apps used with Petalinux. See the `cross_compile` subdirectory for more information.
