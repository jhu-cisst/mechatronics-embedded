# Cross-compiling Linux applications

Cross-compiling an application to run on the Zynq processor requires a CMake toolchain file and a sysroot
that is compatible with the existing Linux build for FPGA V3. Briefly, the sysroot is an image of the embedded
Linux system, such as `/usr/include` and `/usr/lib`. This allows you to link against the correct versions of
the standard Linux libraries, as well as libraries specific to FPGA V3 (e.g., `libfpgav3.so`).

The toolchain file is created by following the instructions in Part 1 below. The sysroot file, called
`fpgav3-sysroot-cortexa9.zip`, is obtained from
[GitHub Releases](https://github.com/jhu-cisst/mechatronics-embedded/releases).
Generally, rather than downloading the ZIP file yourself, it is better to allow the system to automatically
download it, as described below.

Once the toolchain file(s) and sysroot are set up by following the instructions in Part 1, they can be used
to cross-compile your application, as described in Part 2.

## Part 1: Set up toolchain files and sysroot

1. Clone mechatronics-embedded repository to `<source>` directory:

```
git clone https://github.com/jhu-cisst/mechatronics-embedded.git <source>
````

2. Create build directory and run CMake

```
mkdir build
cd build
ccmake ../source
```

3. Set CMake options during configuration

   * USE_PETALINUX_SYSROOT should be ON
     * Note that this will create a CMake option called PETALINUX_SYSROOT_EXTERNAL, which by default will be set to download the latest sysroot; this can be changed to download a different sysroot, or use one that has already been downloaded
   * USE_VITIS should be ON if you wish to use gcc provided by Vitis; otherwise it can be OFF
   * All other options can be OFF

4. Generate project in CMake to create the toolchain files:

   * `toolchain_vitis_fpgav3.cmake`  (if USE_VITIS is ON)
   * `toolchain_clang_fpgav3.cmake`

5. Build the project (e.g., `make`) to automatically download (and unzip) the sysroot

## Part 2: Use toolchain files and sysroot

1. Create a build tree for the application you wish to cross-compile (you can call it whatever you want; in the following,
we use `appbuild`):

```
mkdir appbuild
cd appbuild
```

2. Run CMake according to the documentation in the toolchain file; typical use is:

```
cmake -DCMAKE_TOOLCHAIN_FILE=<toolchain-file> <path-to-source>
```

   * `<toolchain-file>` is the toolchain file (for VITIS/gcc or clang) created in Part 1
   * `<path-to-source>` is the path to your application source code (i.e., your top-level CMakeLists.txt)

3. Configure your project in CMake (as normal)

4. Build your project (as normal)

**IMPORTANT**: The `-DCMAKE_TOOLCHAIN_FILE` must be correctly specified the first time you invoke `cmake`; if not,
it is best to delete the build tree and start over.

## Automated Example

**Note:** If you wish to compile your own application, you can ignore this section.

The top-level [CMakeLists.txt](/CMakeLists.txt) (in the parent directory) automatically performs most of Part 2 for the specific case
of separately cross-compiling the FPGA V3 libraries and applications (source code in [petalinux](/petalinux) subdirectory)
using either of the two supported cross-compiling toolchains: Vitis (gcc) or clang.

The [CMakeLists.txt](/cross_compile/CMakeLists.txt) in this directory specifies how to compile the FPGA V3 libraries and applications (from source
files in the [petalinux](/petalinux) subdirectory); it is analogous to the CMakeLists.txt that would exist in the application
that you wish to cross-compile.

The top-level CMakeLists.txt automatically completes the first two steps in Part 2 above:

1. It creates the following two sub-directories in the build tree, where `cc_vitis` and `cc_clang` correspond to the `appbuild` directory:

   * `cc_vitis`:  for cross-compiling with Vitis (gcc), assuming USE_VITIS is ON
   * `cc_clang`:  for cross-compiling with clang

2. It invokes `cmake` with the correct parameters
   * Note that it does **not** use `add_subdirectory(cross_compile)`, since the toolchain file has to be specified when calling `cmake`.

After running CMake in the top-level project (and `make` to download the sysroot), it is only necessary to change to one of the above
sub-directories and type `make` (or the appropriate build command for the platform).

Once the libraries and apps are built, they can be deployed on the Zynq by copying the library, `libfpgav3.so`,
to the `/usr/lib` directory (with version number appended, e.g., `libfpgav3.so.1.0`) and the apps to
the `/usr/bin` directory.
