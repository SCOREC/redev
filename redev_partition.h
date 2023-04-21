#ifndef REDEV_REDEV_PARTITION_H
#define REDEV_REDEV_PARTITION_H
#include <adios2.h>
#include <variant>
namespace redev {

/**
 * The PartitionInterface class provides an abstract interface that is used by
 * Redev for sharing partition information among the server and client
 * processes. Instances of the PartitionInterface class define an interface
 * (i.e., GetRank(...)) to query which process owns a point in the domain
 * (RCBPtn) or a given geometric model entity (ClassPtn). Defining the cut tree
 * for RCB or the assignment of ranks to geometric model entities is not the
 * purpose/responsiblity of this class or the RCBPtn and ClassPtn derived
 * classes.
 * @note this class is for exposition only
 */
class PartitionInterface {
public:
  /**
   * Write the partition to the provided ADIOS engine and io.
   * @param[in] eng an ADIOS2 Engine opened in write mode
   * @param[in] io the ADIOS2 IO object that created eng
   */
  virtual void Write(adios2::Engine &eng, adios2::IO &io) = 0;
  /**
   * Read the partition to the provided ADIOS engine/io.
   * @param[in] eng an ADIOS2 Engine opened in read mode
   * @param[in] io the ADIOS2 IO object that created eng
   */
  virtual void Read(adios2::Engine &eng, adios2::IO &io) = 0;
  /**
   * Send the partition information from the root rank to all other ranks in
   * comm.
   * @param[in] comm MPI communicator containing the ranks that need the
   * partition information
   * @param[in] root the source rank that sends the partition information
   */
  virtual void Broadcast(MPI_Comm comm, int root = 0) = 0;
};
/**
 * The ClassPtn class supports a domain partition defined by the ownership of
 * geometric model entities. The user passes to the constructor the assignment
 * of ranks to geometric model entities. The 'Class' in the name is from the
 * 'classification' term used to define the association of mesh entities with a
 * geometric model entity. The concepts of classification are described in
 * Section 2.2.2 of the PUMI Users Guide (v2.1)
 * https://www.scorec.rpi.edu/pumi/PUMI.pdf.
 */
class ClassPtn {
public:
  /**
   * Pair of integers (dimension, id) that uniquely identify a geometric model
   * entity by their dimension and id.  The id for an entity within a dimension
   * is unique.
   */
  using ModelEnt = std::pair<redev::LO, redev::LO>;
  /**
   * Vector of geometric model entities.
   */
  using ModelEntVec = std::vector<ModelEnt>;
  /**
   * Map of geometric model entities to the process that owns them.
   */
  using ModelEntToRank = std::map<ModelEnt, redev::LO>;
  ClassPtn();
  /**
   * Create a ClassPtn object from a vector of owning ranks and geometric model
   * entities.
   * @param[in] comm MPI communicator containing the ranks that need the
   * partition information
   * @param[in] ranks vector of ranks owning each geometric model entity
   * @param[in] ents vector of geometric model entities
   */
  ClassPtn(MPI_Comm comm, const redev::LOs &ranks, const ModelEntVec &ents);
  /**
   * Return the rank owning the given geometric model entity.
   * @param[in] ent the geometric model entity
   */
  [[nodiscard]] redev::LO GetRank(ModelEnt ent) const;
  void Write(adios2::Engine &eng, adios2::IO &io);
  void Read(adios2::Engine &eng, adios2::IO &io);
  void Broadcast(MPI_Comm comm, int root = 0);
  /**
   * Return the vector of owning ranks for all geometric model entity.
   */
  [[nodiscard]] redev::LOs GetRanks() const;
  /**
   * Return the vector of all geometric model entities.
   */
  [[nodiscard]] ModelEntVec GetModelEnts() const;

private:
  const std::string entsAndRanksVarName = "class partition ents and ranks";
  /**
   * return a vector with the contents of the ModelEntToRank map
   * the map is serialized as [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ...,
   * dim_n-1, id_n-1, rank_n-1]
   */
  void Gather(MPI_Comm comm, int root = 0);
  /**
   * return a vector with the contents of the ModelEntToRank map
   * the map is serialized as [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ...,
   * dim_n-1, id_n-1, rank_n-1]
   */
  [[nodiscard]] redev::LOs SerializeModelEntsAndRanks() const;
  /**
   * Given a vector that contains the owning ranks and geometric model entities
   * [dim_0, id_0, rank_0, dim_1, id_1, rank_1, ..., dim_n-1, id_n-1, rank_n-1]
   * construct the ModelEntToRank map
   */
  [[nodiscard]] ModelEntToRank
  DeserializeModelEntsAndRanks(const redev::LOs &serialized) const;
  /**
   * Ensure that the dimensions of the model ents is [0:3]
   */
  [[nodiscard]] bool ModelEntDimsValid(const ModelEntVec &ents) const;
  /**
   * The map of geometric model entities to their owning rank
   */
  ModelEntToRank modelEntToRank;
};

/**
 * The RCBPtn class supports recursive coordinate bisection domain partitions.
 * The user passes to the constructor the definition of the cut tree and the
 * ranks owning each sub-domain. The non-leaf levels of the partition tree have
 * alternating cut dimensions associated with the level starting with 'x'. Each
 * non-leaf node has a coordinate for the position of the cut along the
 * dimension specified by its level. Non-leaf tree node n with cut dimension d
 * and cut coordinate v along dimension d has a left child that covers the
 * sub-domain d < v and a right child that covers the sub-domain d >= v. ranks:
 * labels the leaf nodes in the partition tree from left to right cuts: Array
 * specifying the coordinates associated with the non-leaf nodes in a
 * breath-first traversal order starting at the root and visiting the child
 * nodes at each level from left to right. The root of the cut tree is stored at
 * index 1 and index 0 is unused. See test_query.cpp for examples.
 */
class RCBPtn {
public:
  RCBPtn();
  /**
   * Create a RCBPtn object with only the dimension specified.  The cuts and
   * ranks will be filled in during Redev creation.
   * @param[in] dim the dimension of the domain (2=2d,3=3d); defines the length
   * of each cut vector
   */
  RCBPtn(redev::LO dim);
  /**
   * Create a RCBPtn object from a vector of owning ranks and cut vectors.
   * @param[in] dim the dimension of the domain (2=2d,3=3d); defines the length
   * of each cut vector
   * @param[in] ranks vector of ranks owning each sub-domain in the cut tree
   * @param[in] cuts vector of 2-vectors or 3-vectors, for 2d and 3d domains
   * respectively, defining the cut tree. The 2-vectors (3-vectors) are stored
   * in the vector as (x0,y0(,z0),x1,y1(,z1),...xN-1,yN-1(,zN-1))
   */
  RCBPtn(redev::LO dim, std::vector<int> &ranks, std::vector<double> &cuts);
  /**
   * Return the rank owning the given point.
   * @param[in] pt the cartesian point in space. The 3rd value is ignored for 2d
   * domains.
   */
  redev::LO GetRank(std::array<redev::Real, 3> &pt) const;
  void Write(adios2::Engine &eng, adios2::IO &io);
  void Read(adios2::Engine &eng, adios2::IO &io);
  void Broadcast(MPI_Comm comm, int root = 0);
  /**
   * Return the vector of owning ranks for each sub-domain of the cut tree.
   */
  [[nodiscard]] std::vector<redev::LO> GetRanks() const;
  /**
   * Return the vector of cuts defining the cut tree. The 2-vectors (3-vectors)
   * are stored in the vector as (x0,y0(,z0),x1,y1(,z1),...xN-1,yN-1(,zN-1))
   */
  [[nodiscard]] std::vector<redev::Real> GetCuts() const;

private:
  const std::string ranksVarName = "rcb partition ranks";
  const std::string cutsVarName = "rcb partition cuts";
  redev::LO dim;
  std::vector<redev::LO> ranks;
  std::vector<redev::Real> cuts;
};

using Partition = std::variant<ClassPtn, RCBPtn>;

} // namespace redev

#endif // REDEV__REDEV_PARTITION_H
