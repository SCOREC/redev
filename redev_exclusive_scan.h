#ifndef REDEV_EXCLUSIVE_SCAN_H
#define REDEV_EXCLUSIVE_SCAN_H
namespace redev {
  template <typename InputIt, typename OutputIt, typename T>
  OutputIt exclusive_scan(InputIt first, InputIt last, OutputIt d_first, T init) {
    (*d_first) = init;
    ++d_first;
    T last_val = init;
    last = std::prev(last);
    for(auto it = first; it!=last; ++it) {
      *d_first = last_val + *it;
      last_val = last_val + *it;
      ++d_first;
    }
  return d_first;
  }
}
#endif
