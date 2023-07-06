FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " file://fpgav3init.service"

inherit update-rc.d systemd

INITSCRIPT_NAME = "fpgav3init"
INITSCRIPT_PARAMS = "start 99 S ."

SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN} = "fpgav3init.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

DEPENDS += "libgpiod"
LDLIBS += " -lgpiod "

EXTRA_OEMAKE = '"LDLIBS=${LDLIBS}"'

do_install:append() {
    if ${@bb.utils.contains('DISTRO_FEATURES', 'sysvinit', 'true', 'false', d)}; then
        install -d ${D}${sysconfdir}/init.d/
        install -m 0755 ${WORKDIR}/fpgav3init ${D}${sysconfdir}/init.d/
    fi

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/fpgav3init.service ${D}${systemd_system_unitdir}
}

FILES:${PN} += "${@bb.utils.contains('DISTRO_FEATURES','sysvinit','${sysconfdir}/*', '', d)}"
