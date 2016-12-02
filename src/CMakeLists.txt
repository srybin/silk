cmake_minimum_required(VERSION 2.8)

set(TPF_LIBRARY_SOURCES ${SOURCES_DIRECTORY}/tpf)
add_subdirectory( tpf )

set(TPF_TESTS_SOURCES ${SOURCES_DIRECTORY}/tests)
#add_subdirectory(tests)