
COMMON_FEATURES:append = "\
    ssh-server-dropbear \
    debug-tweaks \
    "

COMMON_FEATURES:remove = "\
    ssh-server-openssh \
    "

IMAGE_INSTALL:append = "\
    dnf \
    haveged \
    bootgen \
    bootgen-dev \
    ethtool \
    bash-completion \
    grep \
    libgpiod \
    libgpiod-dev \
    packagegroup-core-ssh-dropbear \
    tcpdump \
    avahi-daemon \
    gpio-demo \
    "

IMAGE_INSTALL:remove = "\
    tcf-agent \
    "

inherit extrausers

# root password is dvrk (hashed)
ROOT_PASSWD = "\$6\$xx\$8CuZAVXZuVqlsWyD0X6Qk.lX9eX4DpRjMN2QpHGOy5aAQ4Nga0NUNvOrpq..nrYAwR5Q3IHDpW8WsW5oXuyjk/"

EXTRA_USERS_PARAMS += "\
    usermod -p '${ROOT_PASSWD}' root; \
    usermod -p '' petalinux; \
    useradd -p '' dvrk; \
    "

USERADDEXTENSION:append = " plnx-useradd-sudoers"

EXTRA_USERS_SUDOERS += "dvrk ALL=(ALL) ALL;"
