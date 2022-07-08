#ifndef REDEV_TYPES_H
#define REDEV_TYPES_H
#include <cstdint>
#include <complex>
#include <vector>

namespace redev {
///Local ordinate, used to count items local to a process
typedef std::int32_t LO;
///Vector of local ordinates
using LOs = std::vector<LO>;
///Global ordinate, used to count items across multiple processes
typedef std::int64_t GO;
///Vector of global ordinates
using GOs = std::vector<GO>;
///Floating point values
typedef double Real;
///Vector of floating point values
using Reals = std::vector<Real>;
///Complex values
typedef std::complex<double> CV;
///Vector of complex values
using CVs = std::vector<CV>;
///Identify the current process as part of the Server or a Client
enum class ProcessType {
  Client = 0,
  Server = 1
};
///The ADIOS2 Engine
enum class TransportType {
  BP4 = 0,
  SST = 1
};
}
#endif
