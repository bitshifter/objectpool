cmake_minimum_required(VERSION 2.8.12)

project(OBJECT_POOL)

if(MSVC)
	if((MSVC_VERSION LESS 1800))
		message(FATAL_ERROR "Only MSVC12 and newer are supported.")
	else()
		add_compile_options(-Zi -W4 -WX)
	endif()
else()
	add_compile_options(-g -std=c++11 -Wall -Wextra -Werror)
endif()

set(CPPSRCS
	src/object_pool.cpp
	)

set(CPPHDRS
	src/object_pool.hpp
	)

add_executable(tests ${CPPHDRS} ${CPPSRCS} test/main.cpp)
target_include_directories(tests PRIVATE thirdparty/Catch)
target_compile_definitions(tests PRIVATE -DUNIT_TESTS)
set_target_properties(tests PROPERTIES OUTPUT_NAME test)

find_package(Threads REQUIRED)
add_executable(bench ${CPPHDRS} ${CPPSRCS} bench/main.cpp)
target_link_libraries(bench PRIVATE ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(bench PRIVATE src thirdparty/nonius)

# compare performance against boost object_pool if available
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost COMPONENTS system)
if(Boost_FOUND)
	target_compile_definitions(bench PRIVATE -DBENCH_BOOST_POOL)
	target_include_directories(bench PRIVATE ${Boost_INCLUDE_DIRS})
	target_link_libraries(bench PRIVATE ${Boost_LIBRARIES})
endif()
