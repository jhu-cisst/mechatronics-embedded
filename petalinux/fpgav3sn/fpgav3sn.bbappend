FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

DEPENDS += "libfpgav3"
LDLIBS += " -lfpgav3 "

EXTRA_OEMAKE = '"LDLIBS=${LDLIBS}"'
