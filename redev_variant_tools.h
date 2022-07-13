#ifndef REDEV__REDEV_VARIANT_TOOLS_H
#define REDEV__REDEV_VARIANT_TOOLS_H
namespace wdmcpl {
// see visit entry on cppreference
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
} // namespace wdmcpl

#endif // REDEV__REDEV_VARIANT_TOOLS_H
