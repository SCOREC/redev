cmake_minimum_required(VERSION 3.15.0...3.21.0)

project(redev VERSION 1.2.5 LANGUAGES C CXX) #C is required to find MPI_C

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

include(cmake/CheckGit.cmake)
CheckGitSetup()

option(ENABLE_ASAN "enable address sanitizer" OFF)
set(HAS_ASAN OFF)
if(ENABLE_ASAN AND CMAKE_COMPILER_IS_GNUCXX MATCHES 1)
  set(HAS_ASAN ON)
endif()

find_package(MPI REQUIRED)
#adios2 adds C and Fortran depending on how it was built
find_package(ADIOS2 CONFIG 2.7.1 REQUIRED)

set(REDEV_HEADERS
  redev.h
  redev_types.h
  redev_comm.h
  redev_profile.h
  redev_time.h
  )

set(REDEV_SOURCES
  redev.cpp
  redev_profile.cpp
  redev_time.cpp
  )

add_library(redev ${REDEV_SOURCES})
target_compile_features(redev PUBLIC cxx_std_20)
target_link_libraries(redev PRIVATE redev_git_version)
target_link_libraries(redev PUBLIC adios2::cxx11_mpi MPI::MPI_C)
if(HAS_ASAN)
  target_compile_options(redev PRIVATE -fsanitize=address -fno-omit-frame-pointer)
endif()

include(CTest)
if(BUILD_TESTING)
  message(STATUS "MPIEXEC_EXECUTABLE: ${MPIEXEC_EXECUTABLE}")
  message(STATUS "MPIEXEC_NUMPROC_FLAG: ${MPIEXEC_NUMPROC_FLAG}")

  set(ADIOS2_BPLS_PATH CACHE FILEPATH "Path to the ADIOS2 bpls utility")
  if(ADIOS2_BPLS_PATH AND NOT EXISTS ${ADIOS2_BPLS_PATH})
    message(FATAL_ERROR "ADIOS2_BPLS_PATH ${ADIOS2_BPLS_PATH} is inaccessible")
  endif()
  message(STATUS "ADIOS2_BPLS_PATH: ${ADIOS2_BPLS_PATH}")

  file(COPY "adios2_bp4.yaml" DESTINATION ${CMAKE_BINARY_DIR})
  file(RENAME "${CMAKE_BINARY_DIR}/adios2_bp4.yaml"
              "${CMAKE_BINARY_DIR}/adios2.yaml")

  #somewhat of a hack - want the yaml file in the same dir as the binaries
  install(FILES "adios2_bp4.yaml" RENAME "adios2.yaml" TYPE BIN)

  function(add_exe NAME)
    add_executable(${NAME} ${NAME}.cpp)
    target_link_libraries(${NAME} redev)
    if(HAS_ASAN)
      target_compile_options(${NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
      target_link_libraries(${NAME} asan rt)
    endif()
  endfunction(add_exe)

  function(removeBpFiles TESTNAME)
    add_test(NAME ${TESTNAME} COMMAND ${CMAKE_COMMAND}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/removeBpFiles.cmake)
  endfunction()

  function(mpi_test TESTNAME PROCS EXE)
    removeBpFiles(${TESTNAME}_cleanup)
    add_test(
      NAME ${TESTNAME}
      COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${PROCS} ${VALGRIND} ${VALGRIND_ARGS} ${EXE} ${ARGN}
    )
  endfunction(mpi_test)

  function(dual_mpi_test TESTNAME PROCS1 EXE1 ARGS1 PROCS2 EXE2 ARGS2)
    removeBpFiles(${TESTNAME}_cleanup)
    add_test(NAME ${TESTNAME}
      COMMAND ${CMAKE_COMMAND}
      -DMPIEXEC_EXECUTABLE=${MPIEXEC_EXECUTABLE}
      -DMPIEXEC_NUMPROC_FLAG=${MPIEXEC_NUMPROC_FLAG}
      -DPROCS1=${PROCS1} -DEXE1=${EXE1} -DARGS1=${ARGS1}
      -DPROCS2=${PROCS2} -DEXE2=${EXE2} -DARGS2=${ARGS2}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/runTwoMpiJobs.cmake)
  endfunction(dual_mpi_test)

  add_exe(util_benchsr util_benchsr.cpp)
  add_exe(util_benchsrLarge util_benchsrLarge.cpp)

  add_exe(test_1d test_1d.cpp)
  mpi_test(test_1d_2p 2 ./test_1d)
  add_exe(test_rcb test_rcb.cpp)
  mpi_test(test_rcb_4p 4 ./test_rcb)
  add_exe(test_classPtn test_classPtn.cpp)
  mpi_test(test_classPtn_4p 4 ./test_classPtn)
  add_exe(test_init test_init.cpp)
  mpi_test(test_init_1p 1 ./test_init)
  add_exe(test_setup_rcbPtn test_setup_rcbPtn.cpp)
  dual_mpi_test(test_setup_rcbPtn_1p 1 ./test_setup_rcbPtn 1 1 ./test_setup_rcbPtn 0)
  add_exe(test_setup_classPtn test_setup_classPtn.cpp)
  dual_mpi_test(test_setup_classPtn_1p 1 ./test_setup_classPtn 1 1 ./test_setup_classPtn 0)
  add_exe(test_query test_query.cpp)
  mpi_test(test_query_1p 1 ./test_query)
  add_exe(test_profile test_profile.cpp)
  mpi_test(test_profile_1p 1 ./test_profile)
  add_exe(test_send test_send.cpp)
  dual_mpi_test(test_send_3p
                4 ./test_send 1
                3 ./test_send 0)
  if(ADIOS2_BPLS_PATH)
    #TODO somehow skip these tests (instead of 'not run') if SST is used...
    add_test(NAME test_send_check_msgs
      COMMAND ${ADIOS2_BPLS_PATH} -d -n 27 toRendezvous.bp/ foo)
    set_tests_properties(test_send_check_msgs PROPERTIES
      PASS_REGULAR_EXPRESSION "0 0 1 2 2 2 2 1 1 1 2 0 0 0 0 1 1 1 1 2 2 1 1 2 2 2 2"
      REQUIRED_FILES "toRendezvous.bp/") #results in ctest reporting 'not run'
    add_test(NAME test_send_check_offsets
      COMMAND ${ADIOS2_BPLS_PATH} -d -n 5 toRendezvous.bp/ foo_offsets)
    set_tests_properties(test_send_check_offsets PROPERTIES
      PASS_REGULAR_EXPRESSION "0 7 11 21 27"
      REQUIRED_FILES "toRendezvous.bp/")
    add_test(NAME test_send_check_srcRanks
      COMMAND ${ADIOS2_BPLS_PATH} -d -n 12 toRendezvous.bp/ foo_srcRanks)
    set_tests_properties(test_send_check_srcRanks PROPERTIES
      PASS_REGULAR_EXPRESSION "0 0 0 0 2 0 4 0 3 3 8 2"
      REQUIRED_FILES "toRendezvous.bp/")
  endif()
  add_exe(test_sendOneToTwo test_sendOneToTwo.cpp)
  dual_mpi_test(test_sendOneToTwo_3p
                2 ./test_sendOneToTwo 1
                1 ./test_sendOneToTwo 0)
  if(ADIOS2_BPLS_PATH)
    #TODO somehow skip these tests (instead of 'not run') if SST is used...
    add_test(NAME test_sendOneToTwo_3p_check_msgs
      COMMAND ${ADIOS2_BPLS_PATH} -d -n 6 toRendezvous.bp/ foo)
    set_tests_properties(test_sendOneToTwo_3p_check_msgs PROPERTIES
      PASS_REGULAR_EXPRESSION "0 0 1 1 1 1"
      REQUIRED_FILES "toRendezvous.bp/") #results in ctest reporting 'not run'
  endif()
  add_exe(test_sendrecv test_sendrecv.cpp)
  dual_mpi_test(test_sendrecv_3p
                4 ./test_sendrecv 1
                3 ./test_sendrecv 0)
  add_exe(test_pingpong test_pingpong.cpp)
  dual_mpi_test(test_pingpong
                1 ./test_pingpong 1
                1 ./test_pingpong 0)
endif()

## export the library
set_target_properties(redev PROPERTIES
  PUBLIC_HEADER "${REDEV_HEADERS}")
target_include_directories(redev
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
install(
  TARGETS redev redev_git_version
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
