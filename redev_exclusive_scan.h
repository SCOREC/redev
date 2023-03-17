#ifndef REDEV_EXCLUSIVE_SCAN_H
#define REDEV_EXCLUSIVE_SCAN_H
#include <utility>
namespace redev {
template <typename InputIt, typename OutputIt, typename T>
OutputIt exclusive_scan(InputIt first, InputIt last, OutputIt d_first, T init) {
  while (first != last) {
    auto v = init;
    init = init + *first;
    ++first;
    *d_first++ = std::move(v);
  }
  return d_first;
}
} // namespace redev
#endif
