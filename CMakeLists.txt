CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

PROJECT(memorypool)

IF(MSVC)
	IF((MSVC_VERSION LESS 1800))
		MESSAGE(FATAL_ERROR "Only MSVC12 and newer are supported.")
	ELSE()
		ADD_COMPILE_OPTIONS(-Zi -W4 -WX)
	ENDIF()
ELSE()
	ADD_COMPILE_OPTIONS(-g -std=c++11 -Wall -Wextra -Werror)
ENDIF()

SET(CPPSRCS
	src/main.cpp
	src/memory_pool.cpp
	)

SET(CPPHDRS
	src/catch.hpp
	src/memory_pool.hpp
	)

ADD_EXECUTABLE(runtests ${CPPHDRS} ${CPPSRCS})
