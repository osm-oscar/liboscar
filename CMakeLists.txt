cmake_minimum_required(VERSION 3.12)
project(liboscar)

set(CMAKE_MODULE_PATH
${CMAKE_CURRENT_SOURCE_DIR}/cmake/
${CMAKE_CURRENT_SOURCE_DIR}/vendor/sserialize/cmake/
${CMAKE_CURRENT_SOURCE_DIR}/vendor/sserialize/vendor/libdts2/vendor/libratss/cmake/
)

include(AddTargetProperties)

find_package(LIBRT)
find_package(CGALSimple)

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

set(MY_INCLUDE_DIRS
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set(MY_LINK_LIBRARIES
	sserialize
	CGALSimple::CGAL
)

set(MY_COMPILE_DEFINITIONS
	"-DCGAL_HAS_THREADS"
	"-DBOOST_HAS_THREADS"
	${MY_COMPILE_DEFINITIONS}
)

file(GLOB_RECURSE
	LIBOSCAR_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

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
	src/AdvancedOpTree.cpp
	src/CQRFromPolygon.cpp
	src/CQRFromComplexSpatialQuery.cpp
	src/CellDistanceByAnulus.cpp
	src/CellDistanceBySphere.cpp
	src/KVstats.cpp
	src/KVClustering.cpp
	src/KoMaClustering.cpp
)

add_library(${PROJECT_NAME} STATIC
	${LIBOSCAR_SOURCES_CPP}
	${LIBOSCAR_HEADERS}
)

target_link_libraries(${PROJECT_NAME} ${MY_LINK_LIBRARIES}) 
target_compile_definitions(${PROJECT_NAME} PUBLIC ${MY_COMPILE_DEFINITIONS})
target_include_directories(${PROJECT_NAME} PUBLIC ${MY_INCLUDE_DIRS})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
add_target_properties(${PROJECT_NAME} COMPILE_FLAGS -fPIC)
