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

####################
# brotli
####################

set(BROTLI "${CMAKE_SOURCE_DIR}/external/brotli" CACHE STRING "BROTLI source directory")
message(STATUS "Found BROTLI library in ${BROTLI}")
add_subdirectory(${BROTLI})

set(HEADER_FILES  
  ./src/common/buffer.h
  ./src/common/cpu_data.h
  ./src/common/color.h
  ./src/common/file_source.h
  ./src/common/memory_data.h
  ./src/common/options.h
  ./src/common/status.h
  ./src/common/task.h
	./src/geometry/attribute_type.h
  ./src/geometry/attribute.h
  ./src/geometry/attributes.h
  ./src/geometry/bounding_box.h
  ./src/geometry/cell_index.h
  ./src/geometry/chunk.h
  ./src/geometry/hierarchy.h
  ./src/geometry/node.h
  ./src/geometry/point.h
  ./src/geometry/scale_offset.h
  ./src/geometry/vector3.h
  ./src/las/las_header.h
  ./src/las/las_info.h
  ./src/las/las_vlr.h
  ./src/sampler/sampler_state.h
  ./src/sampler/sampler.h
  ./src/sampler/sampler_poisson.h
  ./src/sampler/sampler_random.h
  ./src/utils/attribute_utils.h
  ./src/utils/brotli_utils.h
  ./src/utils/gen_utils.h
  ./src/utils/file_utils.h
  ./src/utils/chunk_utils.h
  ./src/utils/concurrent_writer.h
  ./src/utils/string_utils.h
  ./src/utils/las_utils.h
  ./src/converter/converter.h
)

set(SRC_FILES
  ./src/common/buffer.cpp
  ./src/common/task.cpp
  ./src/geometry/attributes.cpp
  ./src/geometry/bounding_box.cpp
  ./src/geometry/cell_index.cpp
  ./src/geometry/hierarchy.cpp
  ./src/geometry/node.cpp
  ./src/geometry/point.cpp
  ./src/geometry/scale_offset.cpp
  ./src/geometry/vector3.cpp
  ./src/las/las_info.cpp
  ./src/las/las_header.cpp
  ./src/sampler/sampler_poisson.cpp
  ./src/sampler/sampler_random.cpp
  ./src/utils/attribute_utils.cpp
  ./src/utils/brotli_utils.cpp
  ./src/utils/chunk_utils.cpp
  ./src/utils/concurrent_writer.cpp
  ./src/utils/file_utils.cpp
  ./src/utils/gen_utils.cpp
  ./src/utils/las_utils.cpp
  ./src/converter/converter.cpp
)

add_library(potree-converter-cpp SHARED ${SRC_FILES})

target_include_directories(potree-converter-cpp PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  src
  "${LASZIP}/include"
  "${JSON}/single_include"
  "${BROTLI}/include"
)

target_link_libraries(potree-converter-cpp laszip3 brotlienc-static brotlidec-static)

if (UNIX)
	find_package(Threads REQUIRED)
	find_package(TBB REQUIRED)
	
	target_link_libraries(${PROJECT_NAME} Threads::Threads)
	target_link_libraries(${PROJECT_NAME} tbb)

endif (UNIX)
