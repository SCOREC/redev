#include <algorithm>  // std::sort
#include <array>      // std::array
#include <iostream>
#include "redev.h"

/**
 * Unit tests for RCBPtn::GetCandidateRanks() and its local-tolerance factor.
 *
 * Handcoded partitions:
 *  2D: dim=2, ranks={0,1,2,3}, cuts={0, 0.5, 0.75, 0.25}
 *      rank0={x<0.5,y<0.75}   rank1={x<0.5,y>=0.75}
 *      rank2={x>=0.5,y<0.25}  rank3={x>=0.5,y>=0.25}
 *  1D: dim=1, ranks={0,1,2,3}, cuts={0, 0.5, 0.25, 0.75}
 *      rank0={x<0.25}  rank1=[0.25,0.5)  rank2=[0.5,0.75)  rank3={x>=0.75}
 *
 * The explicit-factor API is GetCandidateRanks(pt, tolFactor), where
 *   tol at node (d,v) = max(tolFactor * localSpacing(d,v), epsGuard).
 * tolFactor=0 -> exact cut-plane detection only (epsGuard floors tol).
 */
namespace {
  redev::LOs Set(std::initializer_list<redev::LO> v) {
    redev::LOs s(v);
    std::sort(s.begin(), s.end());
    return s;
  }
  struct Probe {
    std::array<redev::Real, 3> pt;
    redev::Real tolFactor;   // 0 = exact cuts only
    redev::LOs expected;     // sorted
    const char* desc;
  };
  void checkCandidates(const redev::RCBPtn& ptn, const char* which,
                       const std::vector<Probe>& probes) {
    for (const auto& p : probes) {
      auto pt = p.pt;
      auto actual = ptn.GetCandidateRanks(pt, p.tolFactor);
      std::sort(actual.begin(), actual.end());
      const bool ok = (actual == p.expected);
      std::cout << (ok ? "PASS" : "FAIL") << "  candidates={";
      for (std::size_t i = 0; i < actual.size(); i++)
        std::cout << (i ? "," : "") << actual[i];
      std::cout << "}  expected={";
      for (std::size_t i = 0; i < p.expected.size(); i++)
        std::cout << (i ? "," : "") << p.expected[i];
      std::cout << "}  " << p.desc << "\n";
      REDEV_ALWAYS_ASSERT(ok);
    }
    std::cout << which << ": " << probes.size() << " candidate probes done\n";
  }
} // namespace

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  {
    {  // 1D partition
      const auto dim = 1;
      redev::LOs ranks = {0, 1, 2, 3};
      redev::Reals cuts = {0, 0.5, 0.25, 0.75};
      redev::RCBPtn ptn(dim, ranks, cuts);
      const std::vector<Probe> probes = {
          // tolFactor=0 -> exact cut-plane detection (epsGuard floors tol)
          {{0.1, 0.0, 0.0}, 0.0, Set({0}), "interior rank0"},
          {{0.25, 0.0, 0.0}, 0.0, Set({0, 1}), "exactly on node2 cut"},
          {{0.49, 0.0, 0.0}, 0.0, Set({1}), "interior rank1"},
          {{0.5, 0.0, 0.0}, 0.0, Set({1, 2}), "exactly on root cut"},
          {{0.75, 0.0, 0.0}, 0.0, Set({2, 3}), "exactly on node3 cut"},
          {{0.9, 0.0, 0.0}, 0.0, Set({3}), "interior rank3"},
          // tolFactor=0.04 on uniform partition -> effective tol=0.01
          {{0.5, 0.0, 0.0}, 0.04, Set({1, 2}),
           "small factor keeps exact-cut set"},
          {{0.5, 0.0, 0.0}, 1.04, Set({0, 1, 2, 3}),
           "large factor adds all sub-domains within tol"},
          {{0.495, 0.0, 0.0}, 0.04, Set({1, 2}),
           "tolFactor captures approach to root cut"},
      };
      checkCandidates(ptn, "1D", probes);
      // default (no-arg) tolerance: factor * local inter-cut spacing
      REDEV_ALWAYS_ASSERT(ptn.GetDefaultTolFactor() == 1e-3);
      {
        auto ptNear = std::array<redev::Real, 3>{0.4999, 0.0, 0.0};
        auto ptFar = std::array<redev::Real, 3>{0.499, 0.0, 0.0};
        REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptNear) == Set({1, 2}));
        REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptFar) == Set({1}));
        // the no-arg overload uses the default factor, i.e.
        // GetCandidateRanks(pt) == GetCandidateRanks(pt, defaultTolFactor)
        REDEV_ALWAYS_ASSERT(
            ptn.GetCandidateRanks(ptNear) ==
            ptn.GetCandidateRanks(ptNear, redev::RCBPtn::GetDefaultTolFactor()));
      }
    }
    {  // 2D partition
      const auto dim = 2;
      redev::LOs ranks = {0, 1, 2, 3};
      redev::Reals cuts = {0, 0.5, 0.75, 0.25};
      redev::RCBPtn ptn(dim, ranks, cuts);
      const std::vector<Probe> probes = {
          // tolFactor=0 -> exact cut-plane detection
          {{0.1, 0.1, 0.0}, 0.0, Set({0}), "interior LL"},
          {{0.4, 0.8, 0.0}, 0.0, Set({1}), "interior UL"},
          {{0.6, 0.1, 0.0}, 0.0, Set({2}), "interior LR"},
          {{0.8, 0.9, 0.0}, 0.0, Set({3}), "interior UR"},
          {{0.5, 0.5, 0.0}, 0.0, Set({0, 3}), "on root x-cut"},
          {{0.25, 0.75, 0.0}, 0.0, Set({0, 1}), "on node2 y-cut"},
          {{0.75, 0.25, 0.0}, 0.0, Set({2, 3}), "on node3 y-cut (x interior)"},
          {{0.5, 0.25, 0.0}, 0.0, Set({0, 2, 3}), "on x-cut AND y-cut (corner)"},
          {{0.5, 0.75, 0.0}, 0.0, Set({0, 1, 3}),
           "on root x-cut AND node2 y-cut (corner across subtrees)"},
          {{0.495, 0.5, 0.0}, 0.04, Set({0}),
           "tolFactor on sparse x-axis has no local scale to broaden"},
          {{0.4, 0.745, 0.0}, 0.04, Set({0, 1}),
           "tolFactor captures approach to node2 y-cut"},
          // NOTE: probe coordinates must keep clear margin from the cuts -
          // e.g. 0.5 - 0.49 == 0.010000000000000009 in IEEE doubles (> 0.01),
          // so 'at-tolerance' probes land on the wrong side of the compare.
          {{0.4, 0.72, 0.0}, 0.04, Set({0}), "beyond tolFactor stays interior"},
          {{0.01, 0.01, 0.0}, 0.04, Set({0}),
           "interior even with tolerance factor"},
      };
      checkCandidates(ptn, "2D", probes);
      // default (no-arg) tolerance: local inter-cut spacing + eps guard
      REDEV_ALWAYS_ASSERT(ptn.GetDefaultTolFactor() == 1e-3);
      {
        // On the sparse x-axis (one cut, no neighbor) the local spacing is
        // zero, so the tolerance floor is ~1e-14 — even with a non-zero factor,
        // near-boundary broadening only applies on dimensions with ≥2 cuts.
        auto ptOnX = std::array<redev::Real, 3>{0.4999, 0.5, 0.0};
        REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptOnX) == Set({0}));
        // y-axis has two cuts (0.25/0.75, spacing 0.5), so the factor
        // 5e-4): a 1e-4 approach to the y-cut broadens candidates, 1e-3 does
        // not
        auto ptYNear = std::array<redev::Real, 3>{0.4, 0.7499, 0.0};
        auto ptYFar = std::array<redev::Real, 3>{0.4, 0.749, 0.0};
        REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptYNear) == Set({0, 1}));
        REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptYFar) == Set({0}));
      }
    }
    {  // non-uniform 1D partition: default tolerance is LOCAL, proportional
      // to the inter-cut spacing along the axis.  cuts {0, 0.8, 0.2, 0.9}:
      // local spacing at the 0.2 cut is 0.6 (tol 6e-4) but at the 0.9 cut is
      // 0.1 (tol 1e-4), so the SAME absolute distance (5e-4) to the two
      // planes behaves differently.
      redev::LOs ranks = {0, 1, 2, 3};
      redev::Reals cuts = {0, 0.8, 0.2, 0.9};
      redev::RCBPtn ptn(1, ranks, cuts);
      auto ptCoarse = std::array<redev::Real, 3>{0.1995, 0.0, 0.0};  // 5e-4 below 0.2
      auto ptFine = std::array<redev::Real, 3>{0.8995, 0.0, 0.0};   // 5e-4 below 0.9
      // local default: the coarse-side cut has a wider local tolerance
      REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptCoarse) == Set({0, 1}));
      REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptFine) == Set({2}));
      // same factor applied uniformly — the effective tolerance VARIES per cut
      // (factor*localSpacing): 0.01*0.6=6e-3 at v=0.2, 0.01*0.1=1e-3 at v=0.9,
      // so both points (each 5e-4 from their cut) are within the local tol
      REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptFine, 0.01) == Set({2, 3}));
      REDEV_ALWAYS_ASSERT(ptn.GetCandidateRanks(ptCoarse, 0.01) == Set({0, 1}));
    }
    std::cout << "test_candidateRanks: ALL PASSED\n";
  }
  MPI_Finalize();
  return 0;
}