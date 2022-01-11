#pragma once
#include <redev.h>
#include <type_traits> // is_same

namespace redev {

//TODO I think there is a cleaner way
template<class T>
MPI_Datatype getMpiType(T) {
  MPI_Datatype mpitype;
  //determine the type based on what is being sent
  if( std::is_same<T, redev::Real>::value ) {
    mpitype = MPI_DOUBLE;
  } else if ( std::is_same<T, redev::CV>::value ) {
    //https://www.mpi-forum.org/docs/mpi-3.1/mpi31-report/node48.htm
    mpitype = MPI_CXX_DOUBLE_COMPLEX;
  } else if ( std::is_same<T, redev::GO>::value ) {
    mpitype = MPI_UNSIGNED_LONG;
  } else if ( std::is_same<T, redev::LO>::value ) {
    mpitype = MPI_INT;
  } else {
    assert(false);
    fprintf(stderr, "Unknown type in %s... exiting\n", __func__);
    exit(EXIT_FAILURE);
  }
  return mpitype;
}

template<typename T>
void Broadcast(T* data, int count, int root, MPI_Comm comm) {
  //begin_func();
  auto type = getMpiType(T());
  MPI_Bcast(data, count, type, root, comm);
  //end_func();
}

template<typename T>
class Communicator {
  public:
    virtual void Pack(LOs& dest, LOs& offsets, T* msgs) = 0;
    virtual void Send() = 0;
    virtual void Unpack() = 0;
};

template<typename T>
class AdiosComm : public Communicator<T> {
  public:
    AdiosComm(MPI_Comm comm_, int rdvRanks_, adios2::Engine& eng_, adios2::IO& io_, std::string name_)
      : comm(comm_), rdvRanks(rdvRanks_), eng(eng_), io(io_), name(name_) {
    }
    void Pack(LOs& dest_, LOs& offsets_, T* msgs_) {
      Msg m(dest_, offsets_, msgs_);
      packed.push_back(m);
    }
    void Send() {
      int rank, commSz;
      MPI_Comm_rank(comm, &rank);
      MPI_Comm_size(comm, &commSz);
      // The assumption here is that the communicator
      // used by the rendezvous application is orders
      // of magnitude smaller than the communicator
      // used by the largest application.  For example,
      // XGC may use 1024 ranks, GENE 16, and the coupler
      // (the rendezvous application) 16. Given this,
      // allocating a comm size array is acceptable.
      GOs degree(rdvRanks,0);
      //for now assume pack was called exactly once on each rank
      for( auto p : packed ) {
        for( auto i=0; i<p.dest.size(); i++) {
          auto destRank = p.dest[i];
          assert(destRank < rdvRanks);
          degree[destRank] += p.offsets[i+1] - p.offsets[i];
        }
      }
      GOs gOffset(rdvRanks,0);
      //TODO make a wrapper for scan
      auto ret = MPI_Exscan(degree.data(), gOffset.data(), rdvRanks, 
          getMpiType(redev::GO()), MPI_SUM, comm);
      assert(ret == MPI_SUCCESS);
    }
    void Unpack() {
      //eng.Get(varVersion, inHash);
      //eng.PerformGets();
    }
  private:
    MPI_Comm comm;
    int rdvRanks;
    adios2::Engine eng;
    adios2::IO io;
    std::string name;
    //support only one call to pack for now...
    struct Msg {
      Msg(LOs& dest_, LOs& offsets_, T* msgs_)
        : dest(dest_), offsets(offsets_), msgs(msgs_) { }
      LOs& dest;
      LOs& offsets;
      T* msgs;
    };
    std::vector<Msg> packed;
};

}
