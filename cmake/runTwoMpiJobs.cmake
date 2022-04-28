cmake_minimum_required(VERSION 3.15.0...3.21.0)
function(mpi_test PROCS1 EXE1 ARGS1 
                  PROCS2 EXE2 ARGS2)
  set(options NONE)
  set(oneValueArgs PROCS3 EXE3)
  set(multiValueArgs ARGS3)
  message(STATUS "options ${options}")
  message(STATUS "oneValueArgs ${oneValueArgs}")
  message(STATUS "multiValueArgs ${multiValueArgs}")
  cmake_parse_arguments(PARSE_ARGV 7 MPI_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}")
  if(NOT PROCS3)
    execute_process(
      RESULTS_VARIABLE result
      OUTPUT_VARIABLE stdout
      ERROR_VARIABLE stderr
      COMMAND_ECHO STDOUT
      COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${PROCS1}
              ${VALGRIND} ${VALGRIND_ARGS}
              ${EXE1} ${ARGS1}
      COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${PROCS2}
              ${VALGRIND} ${VALGRIND_ARGS}
              ${EXE2} ${ARGS2}
    )
  elseif(PROCS3)
    execute_process(
      RESULTS_VARIABLE result
      OUTPUT_VARIABLE stdout
      ERROR_VARIABLE stderr
      COMMAND_ECHO STDOUT
      COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${PROCS1}
              ${VALGRIND} ${VALGRIND_ARGS}
              ${EXE1} ${ARGS1}
      COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${PROCS2}
              ${VALGRIND} ${VALGRIND_ARGS}
              ${EXE2} ${ARGS2}
      COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${PROCS3}
              ${VALGRIND} ${VALGRIND_ARGS}
              ${EXE3} ${ARGS3}
    )
  else()
    message(FATAL_ERROR "Either 6 or 9 arguments must be provided for two or three executables")
  endif()

  foreach(res IN LISTS result)
    message(STATUS "res ${res}")
    if(res)
      message(FATAL_ERROR "Error stdout ${stdout} stderr ${stderr}")
    endif()
  endforeach()
endfunction(mpi_test)

mpi_test(${PROCS1} ${EXE1} ${ARGS1} ${PROCS2} ${EXE2} ${ARGS2} 
  MPI_TEST_PROCS3 ${PROCS3} 
  MPI_TEST_EXE3 ${EXE3} 
  MPI_TEST_ARGS3 ${ARGS3})
