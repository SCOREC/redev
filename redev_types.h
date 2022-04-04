#ifndef REDEV_TYPES_H
#define REDEV_TYPES_H
#include <cstdint>
#include <complex>
#include <vector>

namespace redev {
typedef std::int32_t LO;
using LOs = std::vector<LO>;
typedef std::int64_t GO;
using GOs = std::vector<GO>;
typedef double Real;
using Reals = std::vector<Real>;
typedef std::complex<double> CV;
using CVs = std::vector<CV>;
}
#endif
