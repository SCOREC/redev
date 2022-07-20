#ifndef REDEV__REDEV_VARIANT_TOOLS_H
#define REDEV__REDEV_VARIANT_TOOLS_H
namespace redev {
// see visit entry on cppreference
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
} // namespace redev

#endif // REDEV__REDEV_VARIANT_TOOLS_H
