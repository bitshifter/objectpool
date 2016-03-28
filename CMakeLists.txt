CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

PROJECT(object_pool)

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
	src/object_pool.cpp
	)

SET(CPPHDRS
	src/object_pool.hpp
	)

ADD_EXECUTABLE(runtests ${CPPHDRS} ${CPPSRCS} src/catch.hpp src/runtests.cpp)
TARGET_COMPILE_DEFINITIONS(runtests PRIVATE -DUNIT_TESTS)
ADD_EXECUTABLE(runbench ${CPPHDRS} ${CPPSRCS} src/bench.hpp src/runbench.cpp)
TARGET_COMPILE_DEFINITIONS(runbench PRIVATE -DBENCHMARKS)
