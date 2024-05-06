# Cross-compiling Linux applications

This directory contains a `CMakeLists.txt` to cross-compile the FPGA V3 libraries and applications using either of
the two supported cross-compiling toolchains: Vitis (gcc) or clang.

Note that the top-level `CMakeLists.txt` (in the parent directory) does not contain an `add_subdirectory (cross_compile)`
because cross-compiling requires a toolchain file. But, since this project generates the toolchain files,
`toolchain_vitis_fpgav3.cmake` and `toolchain_clang_fpgav3.cmake`, in the build tree, the top-level `CMakeLists.txt`
also creates the following two sub-directories in the build tree that already contain the appropriate build environment
for cross-compiling:

* `cc_vitis`:  for cross-compiling with Vitis (gcc)
* `cc_clang`:  for cross-compiling with clang

After running CMake in the top-level project, it is only necessary to change to one of the above sub-directories
and type `make` (or the appropriate build command for the platform).

Once the libraries and apps are built, they can be deployed on the Zynq by copying the library, `libfpgav3.so`,
to the `/usr/lib` directory (with version number appended, e.g., `libfpgav3.so.1.0`) and the apps to
the `/usr/bin` directory.
