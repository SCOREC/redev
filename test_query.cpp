#include <iostream>
#include <cstdlib>
#include <numeric> //iota
#include <cassert>
#include "redev.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  auto isRdv = true;
  auto noParticipant = true;
  std::cout << "comm rank " << rank << " size " << nproc << " isRdv " << isRdv << "\n";
  { //1D
    const auto dim = 1;
    std::vector<redev::LO> ranks = {0,1,2,3};
    std::vector<redev::Real> cuts = {0,0.5,0.25,0.75};
    auto ptn = redev::RCBPtn(dim,ranks,cuts);
    redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv,noParticipant);
    rdv.Setup();
    redev::Real pt[3] = {0.6, 0.0, 0.0};
    pt[0] = 0.6;   assert(2 == ptn.GetRank(pt));
    pt[0] = 0.01;  assert(0 == ptn.GetRank(pt));
    pt[0] = 0.5;   assert(2 == ptn.GetRank(pt));
    pt[0] = 0.751; assert(3 == ptn.GetRank(pt));
  }
  { //2D
    const auto dim = 2;
    std::vector<redev::LO> ranks = {0,1,2,3};
    std::vector<redev::Real> cuts = {0,0.5,0.75,0.25};
    /* rendezvous domain: cuts (- and | ) and process ids ([0-3])
     *             0.5
     * 1.0       1 |
     *     0.75----|
     * 0.5         | 3
     *             |----0.25
     * 0.0       0 | 2
     *
     *       0.0  0.5  1.0
     */
    auto ptn = redev::RCBPtn(dim,ranks,cuts);
    redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv,noParticipant);
    rdv.Setup();
    redev::Real pt[3] = {0.1, 0.7, 0.0};
    pt[0] = 0.1, pt[1] = 0.7, assert(0 == ptn.GetRank(pt));
    pt[0] = 0.1; pt[1] = 0.8; assert(1 == ptn.GetRank(pt));
    pt[0] = 0.5; pt[1] = 0.0; assert(2 == ptn.GetRank(pt));
    pt[0] = 0.7; pt[1] = 0.9; assert(3 == ptn.GetRank(pt));
  }
  { //3D
    const auto dim = 3;
    std::vector<redev::LO> ranks(8);
    std::iota(ranks.begin(),ranks.end(),0);
    std::vector<redev::Real> cuts = {0,/*x*/0.5,/*y*/0.75,0.25,/*z*/0.1,0.4,0.8,0.3};
    auto ptn = redev::RCBPtn(dim,ranks,cuts);
    redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv,noParticipant);
    rdv.Setup();
    { redev::Real pt[3] = {0.1, 0.7, 0.01}; assert(0 == ptn.GetRank(pt)); }
    { redev::Real pt[3] = {0.1, 0.7, 0.1};  assert(1 == ptn.GetRank(pt)); }
    { redev::Real pt[3] = {0.1, 0.8, 0.1};  assert(2 == ptn.GetRank(pt)); }
    { redev::Real pt[3] = {0.1, 0.8, 0.8};  assert(3 == ptn.GetRank(pt)); }
    { redev::Real pt[3] = {0.6, 0.1, 0.01}; assert(4 == ptn.GetRank(pt)); }
    { redev::Real pt[3] = {0.6, 0.1, 0.9};  assert(5 == ptn.GetRank(pt)); }
    { redev::Real pt[3] = {0.6, 0.8, 0.0};  assert(6 == ptn.GetRank(pt)); }
    { redev::Real pt[3] = {0.6, 0.8, 0.3};  assert(7 == ptn.GetRank(pt)); }
  }

  MPI_Finalize();
  return 0;
}
