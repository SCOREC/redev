#!/bin/bash -x
if [[ $# == 10 || $# == 14 || $# == 20 ]]; then
  # Old format: no preflags
  runCmd=${1}
  numProcsFlag=${2}
  preFlags=""
  shift 2
elif [[ $# == 11 || $# == 15 || $# == 21 ]]; then
  # New format: with preflags
  runCmd=${1}
  preFlags=${2}
  numProcsFlag=${3}
  shift 3
else
  echo "Usage: <run command> <process flag> <preflags> <name0> <procs0> <exe0> <args0> ... <name2> <procs2> <exe2> <args2>" && \
  exit 1
fi

run() {
  local name=${1}
  local procs=${2}
  local exe=${3}
  local args=${4}
  IFS=';' read -a argsArray <<< "${args}" #split the cmake list of args
  ${runCmd} ${preFlags} ${numProcsFlag} ${procs} ${exe} ${argsArray[@]} &> ${name}.log &
  PIDS+=($!)
  LOGS+=(${name}.log)
}

run $@
shift 4
run $@
shift 4
[[ $# == 4 ]] && run $@
for i in "${!PIDS[@]}"; do
  wait ${PIDS[$i]}
  status=$?
  if [[ $status -ne 0 ]]; then
    cat ${LOGS[$i]}
    exit $status
  fi
done
