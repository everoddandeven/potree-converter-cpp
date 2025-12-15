cmake_minimum_required(VERSION 3.16)
set(CMAKE_SUPPRESS_REGENERATION true)

project(potree-converter-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)

####################
# laszip
####################

set(LASZIP "${CMAKE_SOURCE_DIR}/external/laszip" CACHE STRING "LASzip source directory")
message(STATUS "Found LASzip library in ${LASZIP}")
add_subdirectory(${LASZIP})

####################
# json
####################

set(JSON "${CMAKE_SOURCE_DIR}/external/json" CACHE STRING "JSON source directory")
message(STATUS "Found JSON library in ${JSON}")

set(HEADER_FILES  
	./src/common/attribute_type.h
  ./src/common/attribute.h
  ./src/common/attributes.h
  ./src/common/bounding_box.h
  ./src/common/buffer.h
  ./src/common/chunk.h
  ./src/common/cpu_data.h
  ./src/common/color.h
  ./src/common/file_source.h
  ./src/common/hierarchy_builder.h
  ./src/common/las_point_f2.h
  ./src/common/memory_data.h
  ./src/common/node.h
  ./src/common/options.h
  ./src/common/scale_offset.h
  ./src/common/status.h
  ./src/common/vector3.h
  ./src/las/las_header.h
  ./src/las/las_info.h
  ./src/las/las_vlr.h
  ./src/sampler/sampler_cell_index.h
  ./src/sampler/sampler_point.h
  ./src/sampler/sampler_state.h
  ./src/sampler/sampler.h
  ./src/sampler/sampler_random.h
  ./src/utils/gen_utils.h
  ./src/utils/file_utils.h
  ./src/utils/attribute_utils.h
  ./src/utils/chunk_utils.h
  ./src/sampler/string_utils.h
  ./src/sampler/las_utils.h
)

set(SRC_FILES
  ./src/common/attributes.cpp
  ./src/common/bounding_box.cpp
  ./src/common/hierarchy_builder.cpp
  ./src/common/node.cpp
  ./src/common/scale_offset.cpp
  ./src/common/vector3.cpp
  ./src/las/las_info.cpp
  ./src/las/las_header.cpp
  ./src/sampler/sampler_cell_index.cpp
  ./src/sampler/sampler_random.cpp
  ./src/utils/attribute_utils.cpp
  ./src/utils/chunk_utils.cpp
  ./src/utils/file_utils.cpp
  ./src/utils/gen_utils.cpp
  ./src/utils/las_utils.cpp
)

add_library(potree-converter-cpp SHARED ${SRC_FILES})

target_include_directories(potree-converter-cpp PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  src
  "${LASZIP}/include"
  "${JSON}/single_include"
)

target_link_libraries(potree-converter-cpp laszip3)

if (UNIX)
	find_package(Threads REQUIRED)
	find_package(TBB REQUIRED)
	
	target_link_libraries(${PROJECT_NAME} Threads::Threads)
	target_link_libraries(${PROJECT_NAME} tbb)

endif (UNIX)
