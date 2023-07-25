SUMMARY = "libfpgav3"
DESCRIPTION = "Library that supports FPGA1394 V3"
AUTHOR = "Peter Kazanzides, Johns Hopkins University"
HOMEPAGE = "https://github.com/jhu-cisst/mechatronics-embedded/"

SECTION = "libs"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = "file://fpgav3_emio.h \
           file://fpgav3_emio.c \
           file://fpgav3_qspi.h \
           file://fpgav3_qspi.c \
	   file://Makefile \
          "

S = "${WORKDIR}"

inherit pkgconfig

PACKAGE_ARCH = "${MACHINE_ARCH}"
PROVIDES = "fpgav3"
TARGET_CC_ARCH += "${LDFLAGS}"

DEPENDS = "libgpiod"
LDLIBS += " -lgpiod "

EXTRA_OEMAKE = '"LDLIBS=${LDLIBS}"'

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${libdir}
    install -d ${D}${includedir}
    install ${S}/*.h ${D}${includedir}
    oe_libinstall -so libfpgav3 ${D}${libdir}
}

FILES:${PN} = "${libdir}/*.so.* ${includedir}/* ${libdir}/pkgconfig"
