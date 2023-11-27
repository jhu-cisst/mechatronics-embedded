# ESPM Firmware

This directory contains the compiled firmware (`espm.xsvf`) for the Altera FPGA in the ESPM for the dVRK-Si arms (PSM and ECM). The ESPM is responsible for digitizing the arm sensor feedback and sending it back via an LVDS connection. Commands can also be sent to the ESPM via LVDS, for example, to turn on the LEDs on the arms.

The source code for this firmware is not open source, but the LVDS protocol is open.

This binary file is stored in this respository for convenience, so that it can be copied to the MicroSD image. This allows  the same SD to be used either for the FPGA V3.x inside the dVRK or dVRK-Si controller, or for the ESPM programmer attached to the dVRK-Si arm.
The `espm-firmware-config.cmake` file specifies the ESPM firmware version and the location of the firmware file (`espm.xsvf`).

## Releases:

* Version 1:  Initial release
* Version 2:  Improved DS2480B interface for reading instrument information
