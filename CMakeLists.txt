cmake_minimum_required(VERSION 3.6)
project(liboscar)

set(CMAKE_MODULE_PATH
${CMAKE_CURRENT_SOURCE_DIR}/cmake/
${CMAKE_CURRENT_SOURCE_DIR}/vendor/sserialize/cmake/
)

include(AddTargetProperties)
include(set_highest_cpp)

find_package(LIBRT)

if (LIBOSCAR_NO_DATA_REFCOUNTING_ENABLED)
	set(MY_COMPILE_DEFINITIONS "-DWITH_LIBOSCAR_NO_DATA_REFCOUNTING")
	set(SSERIALIZE_UBA_OPTIONAL_REFCOUNTING_ENABLED TRUE CACHE BOOL "" FORCE)
endif(LIBOSCAR_NO_DATA_REFCOUNTING_ENABLED)
set(LIBOSCAR_NO_DATA_REFCOUNTING_ENABLED
	${LIBOSCAR_NO_DATA_REFCOUNTING_ENABLED}
	CACHE
	BOOL
	"Enable optional reference counting for some heavily used structures"
	FORCE
)
add_subdirectory(vendor/sserialize sserialize)

set(LIBOSCAR_INCLUDE_DIR
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	${SSERIALIZE_INCLUDE_DIR}
	CACHE STRING "liboscar include directories"
	FORCE
)

set(LIBOSCAR_LIBRARIES
	${PROJECT_NAME}
	CACHE STRING "liboscar libraries"
	FORCE
)

set(LIBOSCAR_LINK_LIBRARIES
	${PROJECT_NAME}
	${SSERIALIZE_LINK_LIBRARIES}
	CACHE STRING "liboscar link libraries"
	FORCE
)

set(LIBOSCAR_COMPILE_DEFINITIONS
	${SSERIALIZE_COMPILE_DEFINITIONS}
	${MY_COMPILE_DEFINITIONS}
	CACHE STRING "liboscar compile definitions"
	FORCE
)

mark_as_advanced(
	LIBOSCAR_INCLUDE_DIR
	LIBOSCAR_LIBRARIES
	LIBOSCAR_LINK_LIBRARIES
	LIBOSCAR_COMPILE_DEFINITIONS
)

include_directories(
	${LIBOSCAR_INCLUDE_DIR}
)

file(GLOB_RECURSE INCLUDE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

set(LIBOSCAR_SOURCES_CPP
	src/StaticOsmCompleter.cpp
	src/StaticTagStore.cpp
	src/constants.cpp
	src/tagcompleters.cpp
	src/SetOpTreePrivateGeo.cpp
	src/OsmKeyValueObjectStore.cpp
	src/TextSearch.cpp
	src/GeoSearch.cpp
	src/CellOpTree.cpp
	src/AdvancedCellOpTree.cpp
	src/CQRFromPolygon.cpp
	src/CQRFromComplexSpatialQuery.cpp
)

SET(LIBOSCAR_LINK_LIBS
	${SSERIALIZE_LINK_LIBRARIES}
	${LIBRT_LIBRARIES}
	"CGAL"
	"mpfr"
	"gmp"
	"boost_thread"
	"boost_system"
)

add_library(${PROJECT_NAME} STATIC
	${LIBOSCAR_SOURCES_CPP}
	${INCLUDE_HEADERS}
)
target_link_libraries(${PROJECT_NAME} ${LIBOSCAR_LINK_LIBS}) 
target_compile_definitions(${PROJECT_NAME} PUBLIC ${LIBOSCAR_COMPILE_DEFINITIONS})
add_target_properties(${PROJECT_NAME} COMPILE_FLAGS -fPIC)
add_dependencies(${PROJECT_NAME} sserialize)
