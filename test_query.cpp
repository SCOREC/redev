#include <iostream>
#include <cstdlib>
#include <numeric> //iota
#include "redev.h"

int main(int argc, char** argv) {
  int rank, nproc;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  auto isRdv = true;
  auto noParticipant = true;
  std::cout << "comm rank " << rank << " size " << nproc << " isRdv " << isRdv << "\n";
  { //classPtn
    std::vector<redev::LO> ranks = {0,1,2,3};
    const redev::ClassPtn::ModelEntVec modelEnts {{0,0},{1,0},{2,0},{2,1}};
    auto ptn = redev::ClassPtn(ranks,modelEnts);
    redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv,noParticipant);
    using ModelEnt = redev::ClassPtn::ModelEnt;
    REDEV_ALWAYS_ASSERT(0 == ptn.GetRank(ModelEnt({0,0})) );
    REDEV_ALWAYS_ASSERT(1 == ptn.GetRank(ModelEnt({1,0})) );
    REDEV_ALWAYS_ASSERT(2 == ptn.GetRank(ModelEnt({2,0})) );
    REDEV_ALWAYS_ASSERT(3 == ptn.GetRank(ModelEnt({2,1})) );
  }
  { //1D RCB
    const auto dim = 1;
    std::vector<redev::LO> ranks = {0,1,2,3};
    std::vector<redev::Real> cuts = {0,0.5,0.25,0.75};
    auto ptn = redev::RCBPtn(dim,ranks,cuts);
    redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv,noParticipant);
    std::array<redev::Real,3> pt{0.6, 0.0, 0.0};
    pt[0] = 0.6;   REDEV_ALWAYS_ASSERT(2 == ptn.GetRank(pt));
    pt[0] = 0.01;  REDEV_ALWAYS_ASSERT(0 == ptn.GetRank(pt));
    pt[0] = 0.5;   REDEV_ALWAYS_ASSERT(2 == ptn.GetRank(pt));
    pt[0] = 0.751; REDEV_ALWAYS_ASSERT(3 == ptn.GetRank(pt));
  }
  { //2D RCB
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
    std::array<redev::Real,3> pt{0.1, 0.7, 0.0};
    pt[0] = 0.1, pt[1] = 0.7; REDEV_ALWAYS_ASSERT(0 == ptn.GetRank(pt));
    pt[0] = 0.1; pt[1] = 0.8; REDEV_ALWAYS_ASSERT(1 == ptn.GetRank(pt));
    pt[0] = 0.5; pt[1] = 0.0; REDEV_ALWAYS_ASSERT(2 == ptn.GetRank(pt));
    pt[0] = 0.7; pt[1] = 0.9; REDEV_ALWAYS_ASSERT(3 == ptn.GetRank(pt));
  }
  { //3D RCB
    const auto dim = 3;
    std::vector<redev::LO> ranks(8);
    std::iota(ranks.begin(),ranks.end(),0);
    std::vector<redev::Real> cuts = {0,/*x*/0.5,/*y*/0.75,0.25,/*z*/0.1,0.4,0.8,0.3};
    auto ptn = redev::RCBPtn(dim,ranks,cuts);
    redev::Redev rdv(MPI_COMM_WORLD,ptn,isRdv,noParticipant);
    using Point = std::array<redev::Real,3>;
    { Point pt{0.1, 0.7, 0.01}; REDEV_ALWAYS_ASSERT(0 == ptn.GetRank(pt)); }
    { Point pt{0.1, 0.7, 0.1};  REDEV_ALWAYS_ASSERT(1 == ptn.GetRank(pt)); }
    { Point pt{0.1, 0.8, 0.1};  REDEV_ALWAYS_ASSERT(2 == ptn.GetRank(pt)); }
    { Point pt{0.1, 0.8, 0.8};  REDEV_ALWAYS_ASSERT(3 == ptn.GetRank(pt)); }
    { Point pt{0.6, 0.1, 0.01}; REDEV_ALWAYS_ASSERT(4 == ptn.GetRank(pt)); }
    { Point pt{0.6, 0.1, 0.9};  REDEV_ALWAYS_ASSERT(5 == ptn.GetRank(pt)); }
    { Point pt{0.6, 0.8, 0.0};  REDEV_ALWAYS_ASSERT(6 == ptn.GetRank(pt)); }
    { Point pt{0.6, 0.8, 0.3};  REDEV_ALWAYS_ASSERT(7 == ptn.GetRank(pt)); }
  }

  MPI_Finalize();
  return 0;
}
