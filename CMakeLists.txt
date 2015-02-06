cmake_minimum_required(VERSION 2.8.12)

project(memorypool)

add_compile_options(-g -std=c++11 -Wall -Wextra -Werror)

set(CPPSRCS
	src/main.cpp
	src/memory_pool.cpp
	)

set(CPPHDRS
	src/catch.hpp
	src/memory_pool.hpp
	)

add_executable(runtests ${CPPHDRS} ${CPPSRCS})
