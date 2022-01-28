[![CMake test matrix](https://github.com/SCOREC/redev/actions/workflows/cmake.yml/badge.svg)](https://github.com/SCOREC/redev/actions/workflows/cmake.yml)

# redev
rendezvous communication library

# Dependencies

- MPI
- ADIOS2 v2.7.1 or newer
- C++ compiler supporting C++20

# Install

## Environment on SCOREC RHEL7

```
module unuse /opt/scorec/spack/lmod/linux-rhel7-x86_64/Core 
module use /opt/scorec/spack/v0154_2/lmod/linux-rhel7-x86_64/Core 
module load gcc/10.1.0 mpich cmake/3.20.0
export CMAKE_PREFIX_PATH=$PWD/buildAdios2/install #needed for redev build
```

## ADIOS2

```
git clone git@github.com:ornladios/ADIOS2.git
mkdir buildAdios2
cd !$
cmake ../ADIOS2/ -DCMAKE_INSTALL_PREFIX=$PWD/install -DADIOS2_USE_CUDA=OFF
make -j8
make install
export CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH:$PWD/install
```

## Redev

```
git clone git@github.com:SCOREC/redev.git
mkdir buildRedev
cd !$
cmake ../redev -DCMAKE_INSTALL_PREFIX=$PWD/install
make -j8
make install
ctest
```

