#ifndef REDEV_PROFILE_H
#define REDEV_PROFILE_H
#include "redev_time.h"
#include <map>
#include <string> //std::string
#include <iostream> //std::string

namespace redev {
  using ElapsedTime = double;
  class Profiling {
    private:
      static Profiling *global_singleton_profiling;
      using CallTime = std::pair<size_t,ElapsedTime>;
      std::map<std::string, CallTime> callTime;
      Profiling() {}
    public:
      //prevent calls to the copy and move assignment operators and ctors
      //rule of 5 - #c21 - https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
      Profiling(const Profiling&) = delete;             // copy constructor
      Profiling& operator=(const Profiling&) = delete;  // copy assignment
      Profiling(Profiling&&) = delete;                  // move constructor
      Profiling& operator=(Profiling&&) = delete;       // move assignment

      static Profiling *GetInstance() {
        if (global_singleton_profiling == nullptr)
          global_singleton_profiling = new Profiling;
        return global_singleton_profiling;
      }

      ElapsedTime GetTime(std::string name) {
        if( callTime.contains(name) )
          return callTime[name].second;
        else
          return 0;
      }

      ElapsedTime GetCallCount(std::string name) {
        if( callTime.contains(name) )
          return callTime[name].first;
        else
          return 0;
      }

      void AddTime(std::string name, ElapsedTime t) {
        if(!callTime.contains(name)) {
          callTime[name].first = 1;
          callTime[name].second = t;
        } else {
          callTime[name].first += 1;
          callTime[name].second += t;
        }
      }

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

  //the following functions can be used to record
  //the runtime or stack trace of a function
  inline void begin_code(std::string name) {
  }

  inline void end_code(std::string name, redev::ElapsedTime time) {
     auto s = redev::Profiling::GetInstance();
     s->AddTime(name,time);
  }

  //This is heavily based on omega_h/src/Omega_h_profile.hpp.  Thanks Dan.
  //https://github.com/sandialabs/omega_h/blob/a43850787b24f96d50807cee5688f09259e96b75/src/Omega_h_profile.hpp
  struct ScopedTimer {
    TimeType start;
    std::string name;
    ScopedTimer(std::string inName)
      : name(inName), start(getTime()) {
      begin_code(name);
    }
    ~ScopedTimer() {
      std::chrono::duration<double> t = getTime()-start; //cannot be 'auto t'
      end_code(name,t.count());
    }
    ScopedTimer(ScopedTimer const&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer const&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;
  };

} //end redev

#define REDEV_FUNCTION_TIMER \
  redev::ScopedTimer redev_scoped_function_timer(__FUNCTION__)

#endif
