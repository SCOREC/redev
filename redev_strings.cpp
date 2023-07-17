#include "redev_strings.h"
#include "redev_profile.h"
#include <algorithm>      // std::transform

namespace redev {
//Ci = case insensitive
bool isSameCaseInsensitive(std::string s1, std::string s2) {
  std::transform(s1.begin(), s1.end(), s1.begin(), ::toupper);
  std::transform(s2.begin(), s2.end(), s2.begin(), ::toupper);
  return s1 == s2;
}
};
