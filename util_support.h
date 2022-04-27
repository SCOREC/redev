#pragma once
#include <adios2.h>
#include <string>
#include <cassert>

namespace support{
  void openEnginesBP4(bool isRendezvous, std::string c2sName, adios2::IO& c2sIO, adios2::Engine& c2sEngine) {
    //create the engine writers at the same time - BP4 does not wait for the readers (SST does)
    if(!isRendezvous) {
      c2sEngine = c2sIO.Open(c2sName, adios2::Mode::Write);
      assert(c2sEngine);
    }
    //create engines for reading
    if(isRendezvous) {
      c2sEngine = c2sIO.Open(c2sName, adios2::Mode::Read);
      assert(c2sEngine);
    }
  }

  void openEnginesSST(bool isRendezvous, std::string c2sName, adios2::IO& c2sIO, adios2::Engine& c2sEngine) {
    //create one engine's reader and writer pair at a time - SST blocks on open(read)
    if(isRendezvous) {
      c2sEngine = c2sIO.Open(c2sName, adios2::Mode::Read);
      assert(c2sEngine);
    } else {
      c2sEngine = c2sIO.Open(c2sName, adios2::Mode::Write);
      assert(c2sEngine);
    }
  }
} //end anonymous namespace

