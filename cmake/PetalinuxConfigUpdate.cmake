# CMake script to update Petalinux config file
#
# Caller must define the following two variables:
#   CONFIG_FILE    existing config file to update
#   FRAGMENT_FILE  file with config update fragments
#
# The basic process is:
#  1) Check CONFIG_FILE to determine whether FLASH is defined as BANKLESS
#     (Petalinux up to 2024.1) or not (Petalinux 2024.2)
#  2) If FLASH not BANKLESS, remove BANKLESS_ from settings in FRAGMENT_FILE
#  3) Append (possibly updated) contents of FRAGMENT_FILE to CONFIG_FILE

file (READ ${CONFIG_FILE} CONFIG_CONTENTS)

# Determine whether BANKLESS appears in CONFIG_SUBSYSTEM_FLASH
string (FIND "${CONFIG_CONTENTS}" "CONFIG_SUBSYSTEM_FLASH_PS7_QSPI_0_BANKLESS_SELECT=y" HAS_BANKLESS)
string (FIND "${CONFIG_CONTENTS}" "CONFIG_SUBSYSTEM_FLASH_PS7_QSPI_0_SELECT=y" HAS_NO_BANKLESS)
if ((HAS_BANKLESS EQUAL -1) AND (HAS_NO_BANKLESS EQUAL -1))
  message (WARNING "PetalinuxConfigUpdate:could not detect FLASH settings in ${CONFIG_FILE}, assuming BANKLESS")
  set (IS_BANKLESS ON)
elseif ((NOT HAS_NO_BANKLESS EQUAL -1) AND (NOT HAS_BANKLESS EQUAL -1))
  message (WARNING "PetalinuxConfigUpdate: detected both bankless and non-bankless FLASH entries in ${CONFIG_FILE}, assuming not BANKLESS")
  set (IS_BANKLESS OFF)
elseif (NOT HAS_NO_BANKLESS EQUAL -1)
  message ("PetalinuxConfigUpdate: FLASH config is not bankless")
  set (IS_BANKLESS OFF)
else ()
  # NOT HAS_BANKLESS EQUAL -1
  message ("PetalinuxConfigUpdate: FLASH config is BANKLESS")
  set (IS_BANKLESS ON)
endif ()

# Using file(READ) and string(REPLACE) instead of configure_file(), since we do not really
# need the output file (instead, we can append the string to the existing config file).
file (READ ${FRAGMENT_FILE} FRAGMENT_CONTENTS)
if (NOT IS_BANKLESS)
  string (REPLACE "BANKLESS_" "" FRAGMENT_CONTENTS ${FRAGMENT_CONTENTS})
endif ()

# Using file(WRITE) instead of file(APPEND) because in the future, could consider
# making edits to contents of CONFIG_FILE.
file (WRITE ${CONFIG_FILE} ${CONFIG_CONTENTS} ${FRAGMENT_CONTENTS})
