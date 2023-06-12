# Petalinux

This directory contains the files necessary to build a Linux kernel using the Xilinx Petalinux tool. Note that it is necessary to source `settings.sh` (or `settings.csh`) in the Petalinux installation before building the kernel. Currently, Petalinux 2022.2 and 2023.1 are supported.

The petalinux project relies on the following subdirectories:

  * `configs` -- configuration files used during the build process; note that there are separate subdirectories for config files for the 2022.2 and 2023.1 tools
    * To change configuration settings, these source files can be edited, or the CMake `PETALINUX_MENU` option can be turned `ON`, which will cause the menus to be shown during the build process
  * `device-tree` -- the `system-user.dtsi` file used to customize the device tree
  * `fpgav3init` -- an application to initialize the FPGA; it is set to run at startup (with `root` privileges)
  * `fpgav3sn` -- an application to query or program the FPGA serial number in the QSPI flash; it requires `root` privileges

The relevant output files are copied to the `petalinux/SD_Image` directory in the build tree, as described in the [top-level ReadMe](/ReadMe.md#output-files).
