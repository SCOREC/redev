cmake_minimum_required(VERSION 3.15.0...3.21.0)
file(REMOVE_RECURSE toRendezvous.bp fromRendezvous.bp.sst)
file(GLOB bpDirs LIST_DIRECTORIES TRUE *.bp*)
message(STATUS "bpDirs ${bpDirs}")
