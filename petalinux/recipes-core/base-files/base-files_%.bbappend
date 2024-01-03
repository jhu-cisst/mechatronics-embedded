# Automount MicroSD card on /media

# First sed expression removes comment from beginning of existing line
# Second sed expression changes mount point from /media/card to /media

do_install:append() {
  sed -i -e '/mmcblk0p1/s/^#//g' -e 's|/media/card|/media|g' ${D}${sysconfdir}/fstab
}
