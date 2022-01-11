#include<adios2.h>
#include<vector>

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int mpiRank, mpiSize;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);

  adios2::ADIOS adios(MPI_COMM_WORLD);
  adios2::IO io = adios.DeclareIO("Write");
  std::string engineName = "BP4";
  if (!engineName.empty())
  {
    io.SetEngine(engineName);
  }

  adios2::Engine engine = io.Open("foo.bp", adios2::Mode::Write);

  {
  const auto Nx = 2;
  adios2::Dims shape{static_cast<unsigned int>(mpiSize * Nx)};
  adios2::Dims start{static_cast<unsigned int>(mpiRank * Nx)};
  adios2::Dims count{static_cast<unsigned int>(Nx)};
  auto var_i32 = io.DefineVariable<int32_t>("i32", shape, start, count);

  std::vector<int> dat(Nx);
  engine.BeginStep();
  engine.Put(var_i32, dat.data());
  engine.EndStep();
  }

  {
  const auto Nx = 2;
  adios2::Dims shape{static_cast<unsigned int>(mpiSize * Nx)};
  adios2::Dims start{};
  adios2::Dims count{};
  auto var_i32 = io.DefineVariable<int32_t>("i32_scatter", shape, start, count);

  engine.BeginStep();

  start = adios2::Dims{static_cast<size_t>(mpiRank*Nx)};
  count = adios2::Dims{static_cast<size_t>(1)};
  var_i32.SetSelection({start,count});
  int x = 42;
  engine.Put(var_i32, &x);

  start = adios2::Dims{static_cast<size_t>(mpiRank*Nx+1)};
  var_i32.SetSelection({start,count});
  int y = 43;
  engine.Put(var_i32, &y);

  engine.EndStep();
  }


  engine.Close();
 
  MPI_Finalize();
  return 0;
}
