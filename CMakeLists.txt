cmake_minimum_required(VERSION 3.12.0...3.21.0)

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
  add_executable(test_init test_init.cpp)
  target_link_libraries(test_init redev)
  add_test(NAME test_init_1p 
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 1 ./test_init
  )
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
