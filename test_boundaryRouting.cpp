#include <algorithm>  // std::count
#include <array>    // std::array
#include <cmath>    // std::nextafter
#include <cstdlib>  // EXIT_FAILURE / EXIT_SUCCESS
#include <iostream>
#include "redev.h"

/**
 * Distributed point-search routing contract test.
 *
 * The scenario
 * ============
 * The rendezvous app owns the REDEZVOUS partition used to route query points to
 * ranks (handcoded here as a 2D RCBPtn over [0,1]^2).  The receiving
 * application owns the TRUE MESH partition (handcoded 2x2 blocks) whose cut
 * positions (x=0.49, y=0.76) are deliberately OFFSET from the rendezvous cuts
 * (x=0.5 and y=0.75/0.25) to model a realistic mesh whose blocks do not
 * coincide with the rendezvous grid.
 *
 * Expected result (as of writing)
 * ===============================
 *  - Part 1 (rendezvous determinism): PASSES.  redev's answers are
 *    deterministic and follow the documented half-open convention
 *    (pt[d] < cut -> left, pt[d] >= cut -> right).
 *  - Part 2 (candidate routing contract)      : FAILS on 4 of 14 rows.  The
 *    destination set is now formed with RCBPtn::GetCandidateRanks (send-to-all-
 *    candidates routing).  Rows that still FAIL are exactly those where the
 *    true mesh owner's rendezvous sub-domain does NOT share a boundary with
 *    the rendezvous cell containing the point - i.e. rendezvous/mesh
 *    misalignment beyond the rendezvous adjacency structure.  This is the
 *    intended signal: multi-candidate routing resolves boundary AMBIGUITY
 *    within the rendezvous partition but cannot heal MISALIGNMENT between the
 *    rendezvous and mesh partitions.
 *
 * Registered in CMakeLists.txt with WILL_FAIL TRUE so CI stays green while the
 * known gap exists.  Remove WILL_FAIL (and expect this to pass) once the
 * misaligned case is addressed, e.g. via cell->rank-overlap routing or
 * rendezvous partitions aligned with the mesh.
 */
namespace {
  /**
   * Handcoded TRUE mesh partition: 2x2 axis-aligned blocks over [0,1]^2.
   * Cut positions (x=0.49, y=0.76) are deliberately OFFSET from the rendezvous
   * cuts.  Mesh convention: half-open blocks, x < 0.49 -> left column,
   * y < 0.76 -> bottom row.  rank = 2*col + row.
   */
  struct MeshOwnership {
    static redev::LO GetRank(const std::array<redev::Real, 3>& pt) {
      redev::LO col = (pt[0] < 0.49) ? 0 : 1;
      redev::LO row = (pt[1] < 0.76) ? 0 : 1;
      return static_cast<redev::LO>(2 * col + row);
    }
  };

  /** One handcoded query point and its hand-verified expected answers. */
  struct Query {
    const std::array<redev::Real, 3> pt;
    const char* desc;
    /** Hand-verified singular rendezvous answer (rdzv convention). */
    redev::LO rdzvExpected;
    /** Hand-verified true mesh owner (mesh convention). */
    redev::LO meshExpected;
  };

  // 2D rendezvous partition (RCBPtn): regions are
  //   rank0=[0,.5)x[0,.75)  rank1=[0,.5)x[.75,1]
  //   rank2=[.5,1]x[0,.25)  rank3=[.5,1]x[.25,1]
  // 2D mesh partition: regions are (xcut=0.49, ycut=0.76)
  //   mesh0=[0,.49)x[0,.76)  mesh1=[0,.49)x[.76,1]
  //   mesh2=[.49,1]x[0,.76)  mesh3=[.49,1]x[.76,1]
  const std::vector<Query> queries = {
      // --- Interior points (Part 2 must PASS today) ---
      {{0.1, 0.1, 0.0}, "interior (both partitions)", 0, 0},
      {{0.3, 0.3, 0.0}, "interior (both partitions)", 0, 0},
      {{0.4, 0.8, 0.0}, "interior (both partitions)", 1, 1},
      {{0.6, 0.1, 0.0}, "interior (both partitions)", 2, 2},
      {{0.8, 0.9, 0.0}, "interior (both partitions)", 3, 3},
      {{0.48, 0.3, 0.0}, "just inside both (left of mesh-cut 0.49)", 0, 0},
      // --- Exactly on a rendezvous cut/corner (EXPECTED to FAIL) ---
      {{0.5, 0.5, 0.0}, "EXACTLY on rdzv x-cut (0.5)", 3, 2},
      {{0.25, 0.75, 0.0}, "EXACTLY on rdzv y-cut (0.75)", 1, 0},
      {{0.75, 0.25, 0.0}, "EXACTLY on rdzv y-cut (0.25)", 3, 2},
      {{0.5, 0.25, 0.0}, "EXACTLY on rdzv 4-region corner", 3, 2},
      {{0.5, 0.75, 0.0}, "on rdzv x-cut AND y-cut", 3, 2},
      // --- FP-slop / misaligned-strip probes (EXPECTED to FAIL) ---
      {{0.495, 0.5, 0.0}, "rdzv-interior BUT in stagger strip (0.49,0.5)", 0, 2},
      {{std::nextafter(0.5, 0.0), 0.5, 0.0},
       "one ulp below rdzv x-cut (FP slop)", 0, 2},
      // --- Educational: on a rdzv cut yet coincidentally consistent ---
      {{0.5, 0.9, 0.0}, "on rdzv x-cut but mesh row agrees", 3, 3},
  };
} // namespace
int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  {
    // Handcoded rendezvous partition.  cuts: slot 0 unused, then BFS order
    // (root x=0.5, then y=0.75 at node 2, y=0.25 at node 3).
    const redev::LO dim = 2;
    redev::LOs ranks = {0, 1, 2, 3};
    redev::Reals cuts = {0, 0.5, 0.75, 0.25};
    redev::RCBPtn rdzvPtn(dim, ranks, cuts);
    redev::Redev rdv(MPI_COMM_WORLD, redev::Partition{rdzvPtn},
                     redev::ProcessType::Server, /*noClients=*/true);

    std::cout << "test_boundaryRouting: handcoded rendezvous (RCBPtn) vs "
                 "handcoded mesh (offset 2x2 blocks)\n"
                 "row format: PASS/FAIL  candidates={dest ranks}  meshOwner=#  "
                 "description\n\n";

    // ---------------- Part 1: rendezvous determinism ----------------
    // redev's GetRank must be deterministic and identical whether queried
    // directly or through the partition stored inside Redev.
    for (const auto& q : queries) {
      std::array<redev::Real, 3> pt = q.pt;
      const auto destLocal = rdzvPtn.GetRank(pt);
      const auto destRedev =
          std::get<redev::RCBPtn>(rdv.GetPartition()).GetRank(pt);
      REDEV_ALWAYS_ASSERT(destLocal == q.rdzvExpected);
      REDEV_ALWAYS_ASSERT(destRedev == destLocal);
    }
    std::cout << "Part 1 (rendezvous determinism + half-open convention): "
                 "PASSED\n\n";

    // ---------------- Part 2: candidate routing contract ----------------
    // Contract: the destination-rank candidate set returned by
    // GetCandidateRanks must contain the TRUE mesh owner.
    std::size_t numUnrouted = 0;
    for (const auto& q : queries) {
      std::array<redev::Real, 3> pt = q.pt;
      auto dest = std::get<redev::RCBPtn>(rdv.GetPartition())
                      .GetCandidateRanks(pt, /*tolFactor=*/0.0);
      const auto ownerMesh = MeshOwnership::GetRank(pt);
      REDEV_ALWAYS_ASSERT(std::count(dest.begin(), dest.end(), q.rdzvExpected) != 0);
      REDEV_ALWAYS_ASSERT(ownerMesh == q.meshExpected);
      const bool routed = std::count(dest.begin(), dest.end(), ownerMesh) != 0;
      if (!routed) ++numUnrouted;
      std::cout << (routed ? "PASS" : "FAIL") << "  candidates={";
      for (std::size_t i = 0; i < dest.size(); i++)
        std::cout << (i ? "," : "") << dest[i];
      std::cout << "}  meshOwner=" << ownerMesh << "  " << q.desc << "\n";
    }

    if (numUnrouted) {
      std::cerr
          << "\nPart 2 (candidate routing contract): " << numUnrouted << " of "
          << queries.size()
          << " query points have a TRUE mesh owner that is NOT in the candidate\n"
          << "set returned by GetCandidateRanks.  That set contains only the\n"
          << "rendezvous sub-domains sharing a boundary with the point, so these\n"
          << "failures are rows where the mesh owner lies in a different\n"
          << "rendezvous region (misalignment beyond adjacency).  This is the\n"
          << "intended signal: multi-candidate routing resolves boundary\n"
          << "AMBIGUITY within the rendezvous partition but cannot heal\n"
          << "rendezvous/mesh MISALIGNMENT.\n";
      MPI_Finalize();
      return EXIT_FAILURE;
    }
    std::cout << "Part 2 (candidate routing contract) PASSED\n";
  }
  MPI_Finalize();
  return EXIT_SUCCESS;
}