/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

#ifndef FPGAV3_LIB_H
#define FPGAV3_LIB_H

#include <iostream>

#include "fpgav3_version.h"

// Following methods return versions that were used when building libfpgav3
const char *libfpgav3_version();
const char *libfpgav3_git_version();

// Following method prints the supplied version strings, and also prints the library
// version strings (i.e., the versions used when compiling the library) if they are different.
void print_fpgav3_versions(std::ostream &outStr = std::cout,
			   const char *version = FPGAV3_VERSION,
			   const char *git_version = FPGAV3_GIT_VERSION);

#endif // FPGAV3_LIB_H
