FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

DEPENDS += "libfpgav3"
LDLIBS += " -lfpgav3 "

EXTRA_OEMAKE = '"LDLIBS=${LDLIBS}"'

# Create symbolic link fpgav3quad (to fpgav3block)
do_install:append() {
   ln -s -r -f ${D}${bindir}/fpgav3block ${D}${bindir}/fpgav3quad
}
