cmake_minimum_required(VERSION 3.15.0...3.21.0)

project(redev VERSION 3.0.0 LANGUAGES C CXX) #C is required to find MPI_C

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
  redev_assert.h
  redev_strings.h
  )

set(REDEV_SOURCES
  redev.cpp
  redev_profile.cpp
  redev_time.cpp
  redev_assert.cpp
  redev_strings.cpp
  )

add_library(redev ${REDEV_SOURCES})
target_compile_features(redev PUBLIC cxx_std_17)
target_link_libraries(redev PRIVATE redev_git_version)
target_link_libraries(redev PUBLIC adios2::cxx11_mpi MPI::MPI_C)
target_compile_options(redev PRIVATE -Werror=switch)
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

  #workaround for now https://github.com/ornladios/ADIOS2/issues/3261
  set(ADIOS2_HAVE_SST OFF CACHE BOOL "True if ADIOS2 was built with SST enabled")
  message(STATUS "ADIOS2_HAVE_SST ${ADIOS2_HAVE_SST}")

  function(add_exe NAME)
    add_executable(${NAME} ${NAME}.cpp)
    target_link_libraries(${NAME} redev)
    if(HAS_ASAN)
      target_compile_options(${NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
      target_link_libraries(${NAME} asan rt)
    endif()
  endfunction(add_exe)

  function(removeAdiosFiles TESTNAME)
    add_test(NAME ${TESTNAME} COMMAND ${CMAKE_COMMAND}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/removeAdiosFiles.cmake)
  endfunction()

  function(mpi_test TESTNAME PROCS EXE)
    removeAdiosFiles(${TESTNAME}_cleanup)
    add_test(
      NAME ${TESTNAME}
      COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${PROCS} ${VALGRIND} ${VALGRIND_ARGS} ${EXE} ${ARGN}
    )
  endfunction(mpi_test)

  function(dual_mpi_test)
    set(oneValueArgs TESTNAME TIMEOUT NAME1 NAME2 EXE1 EXE2 PROCS1 PROCS2)
    set(multiValueArgs ARGS1 ARGS2)
    cmake_parse_arguments(DUALTEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
    removeAdiosFiles(${DUALTEST_TESTNAME}_cleanup)
    add_test(NAME ${DUALTEST_TESTNAME}
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runMultipleMpiJobs.sh
      ${MPIEXEC_EXECUTABLE}
      ${MPIEXEC_NUMPROC_FLAG}
      ${DUALTEST_NAME1} ${DUALTEST_PROCS1} ${DUALTEST_EXE1} "${DUALTEST_ARGS1}"
      ${DUALTEST_NAME2} ${DUALTEST_PROCS2} ${DUALTEST_EXE2} "${DUALTEST_ARGS2}")
    set_tests_properties(${DUALTEST_TESTNAME} PROPERTIES TIMEOUT ${DUALTEST_TIMEOUT})
  endfunction(dual_mpi_test)

  function(tri_mpi_test)
    set(oneValueArgs TESTNAME TIMEOUT NAME1 NAME2 NAME3 EXE1 EXE2 EXE3 PROCS1 PROCS2 PROCS3)
    set(multiValueArgs ARGS1 ARGS2 ARGS3)
    cmake_parse_arguments(TRITEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
    removeAdiosFiles(${TRITEST_TESTNAME}_cleanup)
    add_test(NAME ${TRITEST_TESTNAME}
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runMultipleMpiJobs.sh
      ${MPIEXEC_EXECUTABLE}
      ${MPIEXEC_NUMPROC_FLAG}
      ${TRITEST_NAME1} ${TRITEST_PROCS1} ${TRITEST_EXE1} "${TRITEST_ARGS1}"
      ${TRITEST_NAME2} ${TRITEST_PROCS2} ${TRITEST_EXE2} "${TRITEST_ARGS2}"
      ${TRITEST_NAME3} ${TRITEST_PROCS3} ${TRITEST_EXE3} "${TRITEST_ARGS3}")
    set_tests_properties(${TRITEST_TESTNAME} PROPERTIES TIMEOUT ${TRITEST_TIMEOUT})
  endfunction(tri_mpi_test)

  add_exe(util_benchsr util_benchsr.cpp)
  add_exe(util_benchsrLarge util_benchsrLarge.cpp)

  set(test_timeout 12)
  add_exe(test_1d test_1d.cpp)
  mpi_test(test_1d_2p 2 ./test_1d)
  add_exe(test_rcb test_rcb.cpp)
  mpi_test(test_rcb_4p 4 ./test_rcb)
  add_exe(test_classPtn test_classPtn.cpp)
  mpi_test(test_classPtn_4p 4 ./test_classPtn)
  add_exe(test_classPtnGather test_classPtnGather.cpp)
  mpi_test(test_classPtnGather_2p 2 ./test_classPtnGather)
  add_exe(test_init test_init.cpp)
  mpi_test(test_init_1p 1 ./test_init)
  add_exe(test_initPtnObjOwnership test_initPtnObjOwnership.cpp)
  dual_mpi_test(TESTNAME test_initPtnObjOwnership
    TIMEOUT ${test_timeout}
    NAME1 rdv PROCS1 1 EXE1 ./test_initPtnObjOwnership ARGS1 1
    NAME2 app PROCS2 1 EXE2 ./test_initPtnObjOwnership ARGS2 0)
  add_exe(test_setup_rcbPtn test_setup_rcbPtn.cpp)
  dual_mpi_test(TESTNAME test_setup_rcbPtn_1p
    TIMEOUT ${test_timeout}
    NAME1 rdv PROCS1 1 EXE1 ./test_setup_rcbPtn ARGS1 1
    NAME2 app PROCS2 1 EXE2 ./test_setup_rcbPtn ARGS2 0)
  add_exe(test_setup_classPtn test_setup_classPtn.cpp)
  dual_mpi_test(TESTNAME test_setup_classPtn_1p
    TIMEOUT ${test_timeout}
    NAME1 rdv PROCS1 1 EXE1 ./test_setup_classPtn ARGS1 1
    NAME2 app PROCS2 1 EXE2 ./test_setup_classPtn ARGS2 0)
  add_exe(test_query test_query.cpp)
  mpi_test(test_query_1p 1 ./test_query)
  add_exe(test_profile test_profile.cpp)
  mpi_test(test_profile_1p 1 ./test_profile)
  add_exe(test_send test_send.cpp)
  dual_mpi_test(TESTNAME test_send_3p TIMEOUT ${test_timeout}
    NAME1 rdv PROCS1 4 EXE1 ./test_send ARGS1 1
    NAME2 app PROCS2 3 EXE2 ./test_send ARGS2 0)
  if(ADIOS2_BPLS_PATH)
    add_test(NAME test_send_check_msgs
      COMMAND ${ADIOS2_BPLS_PATH} -d -n 27 foo_c2s.bp/ foo_c2s)
    set_tests_properties(test_send_check_msgs PROPERTIES
      PASS_REGULAR_EXPRESSION "0 0 1 2 2 2 2 1 1 1 2 0 0 0 0 1 1 1 1 2 2 1 1 2 2 2 2"
      REQUIRED_FILES "foo_c2s.bp/") #results in ctest reporting 'not run'
    add_test(NAME test_send_check_offsets
      COMMAND ${ADIOS2_BPLS_PATH} -d -n 5 foo_c2s.bp/ foo_c2s_offsets)
    set_tests_properties(test_send_check_offsets PROPERTIES
      PASS_REGULAR_EXPRESSION "0 7 11 21 27"
      REQUIRED_FILES "foo_c2s.bp/")
    add_test(NAME test_send_check_srcRanks
      COMMAND ${ADIOS2_BPLS_PATH} -d -n 12 foo_c2s.bp/ foo_c2s_srcRanks)
    set_tests_properties(test_send_check_srcRanks PROPERTIES
      ASS_REGULAR_EXPRESSION "0 0 0 0 2 0 4 0 3 3 8 2"
      REQUIRED_FILES "foo_c2s.bp/")
  endif()
  add_exe(test_sendOneToTwo test_sendOneToTwo.cpp)
  dual_mpi_test(TESTNAME test_sendOneToTwo_3p TIMEOUT ${test_timeout}
    NAME1 rdv PROCS1 2 EXE1 ./test_sendOneToTwo ARGS1 1
    NAME2 app PROCS2 1 EXE2 ./test_sendOneToTwo ARGS2 0)
  if(ADIOS2_BPLS_PATH)
    add_test(NAME test_sendOneToTwo_3p_check_msgs
      COMMAND ${ADIOS2_BPLS_PATH} -d -n 6 foo_c2s.bp/ foo_c2s)
    set_tests_properties(test_sendOneToTwo_3p_check_msgs PROPERTIES
      PASS_REGULAR_EXPRESSION "0 0 1 1 1 1"
      REQUIRED_FILES "foo_c2s.bp/") #results in ctest reporting 'not run'
  endif()
  add_exe(test_sendrecv test_sendrecv.cpp)
  dual_mpi_test(TESTNAME test_sendrecv_3p TIMEOUT ${test_timeout}
    NAME1 rdv PROCS1 4 EXE1 ./test_sendrecv ARGS1 1
    NAME2 app PROCS2 3 EXE2 ./test_sendrecv ARGS2 0)
  add_exe(test_pingpong test_pingpong.cpp)
  dual_mpi_test(TESTNAME test_pingpong TIMEOUT ${test_timeout}
    NAME1 rdv PROCS1 1 EXE1 ./test_pingpong ARGS1 1
    NAME2 app PROCS2 1 EXE2 ./test_pingpong ARGS2 0)

  set(isSST 0)
  add_exe(test_twoClients test_twoClients.cpp)
  tri_mpi_test(TESTNAME test_twoClients
    TIMEOUT 12
    NAME1 client0 EXE1 ./test_twoClients PROCS1 1 ARGS1 ${isSST} 0
    NAME2 client1 EXE2 ./test_twoClients PROCS2 1 ARGS2 ${isSST} 1
    NAME3 rdv     EXE3 ./test_twoClients PROCS3 1 ARGS3 ${isSST} -1)

  if(ADIOS2_HAVE_SST)
    set(isSST 1)
    tri_mpi_test(TESTNAME test_twoClients_sst
      TIMEOUT 12
      NAME1 client0 EXE1 ./test_twoClients PROCS1 1 ARGS1 ${isSST} 0
      NAME2 client1 EXE2 ./test_twoClients PROCS2 1 ARGS2 ${isSST} 1
      NAME3 rdv     EXE3 ./test_twoClients PROCS3 1 ARGS3 ${isSST} -1)
  endif()
endif(BUILD_TESTING)

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
