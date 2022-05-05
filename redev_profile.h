#ifndef REDEV_PROFILE_H
#define REDEV_PROFILE_H
#include "redev_time.h"
#include <map>
#include <string> //std::string
#include <iostream> //std::string

namespace redev {
  using ElapsedTime = double;
  /**
   * The Profiling class provides a basic capability to record the call count
   * and time of functions.
   */
  class Profiling {
    private:
      static Profiling *global_singleton_profiling;
      using CallTime = std::pair<size_t,ElapsedTime>;
      std::map<std::string, CallTime> callTime;
      Profiling() {}
    public:
      //prevent calls to the copy and move assignment operators and ctors
      //rule of 5 - #c21 - https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
      //! \{ supress doxygen warning
      Profiling(const Profiling&) = delete;             // copy constructor
      Profiling& operator=(const Profiling&) = delete;  // copy assignment
      Profiling(Profiling&&) = delete;                  // move constructor
      Profiling& operator=(Profiling&&) = delete;       // move assignment
      //! \}

      /**
       * \brief Get the handle to the Profiling singleton instance.
       */
      static Profiling *GetInstance() {
        if (global_singleton_profiling == nullptr)
          global_singleton_profiling = new Profiling;
        return global_singleton_profiling;
      }

      /**
       * \brief Get the time spent in the specified function.
       * @param[in] name the function name, case sensitive
       */
      ElapsedTime GetTime(std::string name) {
        if( callTime.count(name) )
          return callTime[name].second;
        else
          return 0;
      }

      /**
       * \brief Get the call count of the specified function.
       * @param[in] name the function name, case sensitive
       */
      ElapsedTime GetCallCount(std::string name) {
        if( callTime.count(name) )
          return callTime[name].first;
        else
          return 0;
      }

      /**
       * \brief Increment the call count and increase the recorded time for the
       * specified function by t.
       * @param[in] name the function name, case sensitive
       * @param[in] t recorded time in the function
       */
      void AddTime(std::string name, ElapsedTime t) {
        if(!callTime.count(name)) {
          callTime[name].first = 1;
          callTime[name].second = t;
        } else {
          callTime[name].first += 1;
          callTime[name].second += t;
        }
      }

      /**
       * \brief Write profiling data to the specified stream.
       * @param[in/out] os stream object
       */
      void Write(std::ostream& os) const {
        os << "Profiling\n";
        os << "name, callCount, time(s)\n";
        for( auto nameCallsTime : callTime ) {
          auto name = nameCallsTime.first;
          auto calls = nameCallsTime.second.first;
          auto time = nameCallsTime.second.second;
          auto comma = std::string(", ");
          os << name << comma << calls << comma << time << "\n";
        }
      }
  };

  /**
   * \brief Called at the beginning of an instrumented function.
   */
  inline void begin_code(std::string name) {
  }

  /**
   * \brief Called at the end of an instrumented function.
   */
  inline void end_code(std::string name, redev::ElapsedTime time) {
     auto s = redev::Profiling::GetInstance();
     s->AddTime(name,time);
  }

  /**
   * ScopedTimer provides a simple mechanism to record the time spent in the
   * calling scope.
   * This is heavily based on omega_h/src/Omega_h_profile.hpp.  Thanks Dan.
   * https://github.com/sandialabs/omega_h/blob/a43850787b24f96d50807cee5688f09259e96b75/src/Omega_h_profile.hpp
   */
  struct ScopedTimer {
    //! \{ suppress doxygen warning
    TimeType start;
    std::string name;
    //! \}
    /**
     * \brief Time the callers scope and store it with the given name.
     * @param[in] inName function name
     */
    ScopedTimer(std::string inName)
      : name(inName), start(getTime()) {
      begin_code(name);
    }
    //! \{ suppress doxygen warning
    ~ScopedTimer() {
      std::chrono::duration<double> t = getTime()-start; //cannot be 'auto t'
      end_code(name,t.count());
    }
    ScopedTimer(ScopedTimer const&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer const&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;
    //! \}
  };

} //end redev

#define REDEV_FUNCTION_TIMER \
  redev::ScopedTimer redev_scoped_function_timer(__FUNCTION__)

#endif
