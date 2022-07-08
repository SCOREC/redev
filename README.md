[![CMake test matrix](https://github.com/SCOREC/redev/actions/workflows/cmake.yml/badge.svg)](https://github.com/SCOREC/redev/actions/workflows/cmake.yml)

# redev
rendezvous communication library

# Dependencies

- MPI
- ADIOS2 v2.7.1 or newer
- C++ compiler supporting C++17

# Install

## Environment on SCOREC RHEL7

```
module unuse /opt/scorec/spack/lmod/linux-rhel7-x86_64/Core 
module use /opt/scorec/spack/v0154_2/lmod/linux-rhel7-x86_64/Core 
module load gcc/10.1.0 mpich cmake/3.20.0
```

## ADIOS2

```
git clone git@github.com:ornladios/ADIOS2.git
bdir=buildAdios2
cmake -S ADIOS2/ -B $bdir -DCMAKE_INSTALL_PREFIX=$bdir/install -DADIOS2_USE_CUDA=OFF
cmake --build $bdir --target install -j8
```

## Redev

```
git clone git@github.com:SCOREC/redev.git
bdir=buildRedev
export CMAKE_PREFIX_PATH=$PWD/buildAdios2/install:$CMAKE_PREFIX_PATH
cmake -S redev -B $bdir -DCMAKE_INSTALL_PREFIX=$bdir/install
cmake --build $bdir --target install --target test
```

# Using redev

The `example/build_cmake_installed` contains an example `CMakeLists.txt` and
source driver using redev (copied from a test) that can be built against a redev
install with the following commands:

```
cmake -S redev/example/build_cmake_installed/ -B buildRedevExample \
-DCMAKE_CXX_COMPILER=mpicxx -DCMAKE_C_COMPILER=mpicc \
-Dredev_ROOT=/path/to/redev/install \
-DADIOS2_ROOT=/path/to/adios2/install/

cmake --build buildRedevExample
```
