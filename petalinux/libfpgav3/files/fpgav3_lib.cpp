/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#include <string.h>

#include "fpgav3_lib.h"

// Returns versions that were used when building libfpgav3

const char *libfpgav3_version()
{
    return FPGAV3_VERSION;
}

const char *libfpgav3_git_version()
{
    return FPGAV3_GIT_VERSION;
}

void print_fpgav3_versions(std::ostream &outStr, const char *version, const char *git_version)
{
    outStr << "Software Version " << version;
    if (strcmp(git_version, version) != 0) {
        outStr << " (git " << git_version << ")";
    }
    outStr << std::endl;

    // Software version used to build libfpgav3 (if different)
    if (strcmp(git_version, libfpgav3_git_version()) != 0) {
        outStr << "Library Version " << libfpgav3_version();
        if (strcmp(libfpgav3_git_version(), libfpgav3_version()) != 0) {
            outStr << " (git " << libfpgav3_git_version() << ")";
        }
        outStr << std::endl;
    }
}
