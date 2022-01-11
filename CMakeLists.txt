cmake_minimum_required(VERSION 3.15.0...3.21.0)

project(redev VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

include(cmake/CheckGit.cmake)
CheckGitSetup()

find_package(MPI REQUIRED)
#adios2 adds C and Fortran depending on how it was built
find_package(ADIOS2 CONFIG 2.7.1 REQUIRED)

set(REDEV_HEADERS
  redev.h
  redev_types.h
  redev_comm.h
  )

set(REDEV_SOURCES
  redev.cpp
  )

add_library(redev ${REDEV_SOURCES})
target_compile_features(redev PUBLIC cxx_std_20)
target_link_libraries(redev PUBLIC redev_git_version)
target_link_libraries(redev PUBLIC adios2::cxx11_mpi MPI::MPI_C)

include(CTest)
if(BUILD_TESTING)
  message(STATUS "MPIEXEC_EXECUTABLE: ${MPIEXEC_EXECUTABLE}")
  message(STATUS "MPIEXEC_NUMPROC_FLAG: ${MPIEXEC_NUMPROC_FLAG}")

  function(add_exe NAME)
    add_executable(${NAME} ${NAME}.cpp)
    target_link_libraries(${NAME} redev)
  endfunction(add_exe)

  function(mpi_test TESTNAME PROCS EXE)
    add_test(
      NAME ${TESTNAME}
      COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${PROCS} ${VALGRIND} ${VALGRIND_ARGS} ${EXE} ${ARGN}
    )
  endfunction(mpi_test)

  function(dual_mpi_test TESTNAME PROCS1 EXE1 ARGS1 PROCS2 EXE2 ARGS2)
    add_test(NAME ${TESTNAME}
      COMMAND ${CMAKE_COMMAND}
      -DMPIEXEC_EXECUTABLE=${MPIEXEC_EXECUTABLE}
      -DMPIEXEC_NUMPROC_FLAG=${MPIEXEC_NUMPROC_FLAG}
      -DPROCS1=${PROCS1} -DEXE1=${EXE1} -DARGS1=${ARGS1}
      -DPROCS2=${PROCS2} -DEXE2=${EXE2} -DARGS2=${ARGS2}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/runTwoMpiJobs.cmake)
  endfunction(dual_mpi_test)

  add_exe(test_init test_init.cpp)
  mpi_test(test_init_1p 1 ./test_init)
  add_exe(test_setup test_setup.cpp)
  dual_mpi_test(test_setup_1p 1 ./test_setup 1 1 ./test_setup 0)
  add_exe(test_query test_query.cpp)
  mpi_test(test_query_1p 1 ./test_query)
  add_exe(test_sendrecv test_sendrecv.cpp)
  mpi_test(test_sendrecv_3p 3 ./test_sendrecv 1)
endif()

## export the library
set_target_properties(redev PROPERTIES
  PUBLIC_HEADER "${REDEV_HEADERS}")
target_include_directories(redev
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
install(
  TARGETS redev
  EXPORT redev-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/redev-config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/redev
)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/redev-config-version.cmake"
  COMPATIBILITY AnyNewerVersion)

install(FILES
  "${PROJECT_BINARY_DIR}/redev-config.cmake"
  "${PROJECT_BINARY_DIR}/redev-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/redev)

install(
  EXPORT redev-targets
  NAMESPACE redev::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/redev)
