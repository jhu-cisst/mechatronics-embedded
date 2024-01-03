FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " file://myinterfaces"

do_install:append() {
  install -m 0644 ${WORKDIR}/myinterfaces ${D}${sysconfdir}/network/interfaces
}
