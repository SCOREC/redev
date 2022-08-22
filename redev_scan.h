#pragma once

namespace redev {
  template <typename T, typename VT>
  void exclusive_scan(T infirst, T inlast, T outIter, VT init) {
    //TODO use constexpr magic to assert T::value_type is the same as VT
    VT prev = *outIter = init;
    outIter++;
    auto p = infirst;
    while(p != inlast) {
      prev = *outIter = prev + *p;
      outIter++;
      p++;
    }
  }
}

