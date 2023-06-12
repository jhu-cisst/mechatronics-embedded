# Linux Platform

This directory contains the files necessary to create a Linux platform using Vitis. Executables created in this platform can run on the petalinux kernel (alternatively, applications can be compiled within the petalinux build).

The advantages to creating applications here (rather than in the petalinux build) are that (1) it is generally faster to compile the app separately, and (2) it can be compiled using Vitis on a Windows platform (whereas petalinux is only available on Linux).

As an example, the `fpgav3init` application (source code in `petalinux` directory) can be compiled in this Linux platform.
