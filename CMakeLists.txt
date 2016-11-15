cmake_minimum_required(VERSION 3.0)
project(liboscar)

set(CMAKE_MODULE_PATH
${CMAKE_CURRENT_SOURCE_DIR}/cmake/
${CMAKE_CURRENT_SOURCE_DIR}/vendor/sserialize/cmake/
)

INCLUDE(AddTargetProperties)

set(MY_C_FLAGS "-std=c99")
set(MY_CXX_FLAGS "-std=gnu++1y")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MY_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_CXX_FLAGS}")

include_directories(
${CMAKE_SOURCE_DIR}
${CMAKE_CURRENT_BINARY_DIR}
${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/include/
${CMAKE_CURRENT_SOURCE_DIR}/vendor/sserialize/include/
)

find_package(LIBRT REQUIRED)

add_subdirectory(vendor/sserialize sserialize)
add_target_properties(sserialize COMPILE_FLAGS -fPIC)

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
	${sserialize_BINARY_DIR}/libsserialize.a
	${LIBRT_LIBRARIES}
	"CGAL"
	"mpfr"
	"gmp"
	"boost_thread"
	"boost_system"
)

add_library(${PROJECT_NAME} SHARED ${LIBOSCAR_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME} ${LIBOSCAR_LINK_LIBS}) 
add_dependencies(${PROJECT_NAME} sserialize)