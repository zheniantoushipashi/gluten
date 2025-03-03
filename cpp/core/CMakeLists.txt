# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

cmake_minimum_required(VERSION 3.16)

project(gluten)

include(ExternalProject)
include(FindPkgConfig)
include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")

set(BOOST_MIN_VERSION "1.42.0")
find_package(Boost REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

set(source_root_directory ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

set(proto_directory ${CMAKE_CURRENT_SOURCE_DIR}/../../gluten-core/src/main/resources/substrait/proto)
set(substrait_proto_directory ${proto_directory}/substrait)
message(STATUS "Set Proto Directory in ${proto_directory}")

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

# Building Protobuf
macro(build_protobuf)
  message(STATUS "Building Protocol Buffers from Source")

  if(DEFINED ENV{GLUTEN_PROTOBUF_URL})
    set(PROTOBUF_SOURCE_URL "$ENV{GLUTEN_PROTOBUF_URL}")
  else()
    set(PROTOBUF_BUILD_VERSION "21.4")
    set (PROTOBUF_SOURCE_URL
            "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_BUILD_VERSION}/protobuf-all-${PROTOBUF_BUILD_VERSION}.tar.gz")
  endif()
  set(PROTOBUF_BUILD_SHA256_CHECKSUM "6c5e1b0788afba4569aeebb2cfe205cb154aa01deacaba0cd26442f3b761a836")

  set(PROTOBUF_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/protobuf_ep-install")
  set(PROTOBUF_INCLUDE_DIR "${PROTOBUF_PREFIX}/include")
  set(
    PROTOBUF_STATIC_LIB
    "${PROTOBUF_PREFIX}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}protobuf${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
  set(
    PROTOC_STATIC_LIB
    "${PROTOBUF_PREFIX}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}protoc${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
  set(
    PROTOC_BIN
    "${PROTOBUF_PREFIX}/bin/protoc"
    )
  set(
    PROTOBUF_INCLUDE
    "${PROTOBUF_PREFIX}/include"
    CACHE PATH
    "Protobuf include path"
    )
  set(PROTOBUF_COMPILER "${PROTOBUF_PREFIX}/bin/protoc")
  set(PROTOBUF_CONFIGURE_ARGS
      "AR=${CMAKE_AR}"
      "RANLIB=${CMAKE_RANLIB}"
      "CC=${CMAKE_C_COMPILER}"
      "CXX=${CMAKE_CXX_COMPILER}"
      "--disable-shared"
      "--prefix=${PROTOBUF_PREFIX}"
      "CFLAGS=-fPIC"
      "CXXFLAGS=-fPIC")
  set(PROTOBUF_BUILD_COMMAND ${MAKE} ${MAKE_BUILD_ARGS})
  ExternalProject_Add(protobuf_ep
                      PREFIX protobuf_ep
                      CONFIGURE_COMMAND ./autogen.sh COMMAND "./configure" ${PROTOBUF_CONFIGURE_ARGS}
                      BUILD_BYPRODUCTS "${PROTOBUF_STATIC_LIB}" "${PROTOBUF_COMPILER}"
                      BUILD_COMMAND ${PROTOBUF_BUILD_COMMAND}
                      BUILD_IN_SOURCE 1
                      URL ${PROTOBUF_SOURCE_URL}
                      URL_HASH "SHA256=${PROTOBUF_BUILD_SHA256_CHECKSUM}"
  )

  file(MAKE_DIRECTORY "${PROTOBUF_INCLUDE_DIR}")
  add_library(protobuf::libprotobuf STATIC IMPORTED)
  set_target_properties(
    protobuf::libprotobuf
    PROPERTIES IMPORTED_LOCATION "${PROTOBUF_STATIC_LIB}" INTERFACE_INCLUDE_DIRECTORIES
               "${PROTOBUF_INCLUDE_DIR}")
  add_dependencies(protobuf::libprotobuf protobuf_ep)
endmacro()

macro(find_protobuf)
  # Find the existing Protobuf
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  find_package(Protobuf)
  if ("${Protobuf_LIBRARY}" STREQUAL "Protobuf_LIBRARY-NOTFOUND")
    message(FATAL_ERROR "Protobuf Library Not Found")
  endif()
  set(PROTOC_BIN ${Protobuf_PROTOC_EXECUTABLE})
  set(PROTOBUF_INCLUDE "${Protobuf_INCLUDE_DIRS}" CACHE PATH "Protobuf include path")
endmacro()

if(USE_AVX512)
  # Only enable additional instruction sets if they are supported
  message(STATUS "System processor: ${CMAKE_SYSTEM_PROCESSOR}")
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    set(AVX512_FLAG "-march=skylake-avx512")
    check_cxx_compiler_flag(${AVX512_FLAG} CXX_SUPPORTS_AVX512)
    if(NOT CXX_SUPPORTS_AVX512)
      message(FATAL_ERROR "AVX512 required but compiler doesn't support it.")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${AVX512_FLAG}")
    add_definitions(-DCOLUMNAR_PLUGIN_USE_AVX512)
  endif ()
endif()

# Set up Proto
set(PROTO_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)
file(GLOB PROTO_FILES ${proto_directory}/*.proto
                      ${substrait_proto_directory}/*.proto
                      ${substrait_proto_directory}/extensions/*.proto)
FOREACH(PROTO ${PROTO_FILES})
  file(RELATIVE_PATH REL_PROTO ${proto_directory} ${PROTO})
  string(REGEX REPLACE "\\.proto" "" PROTO_NAME ${REL_PROTO})
  LIST(APPEND PROTO_SRCS "${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.cc")
  LIST(APPEND PROTO_HDRS "${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.h")
ENDFOREACH()
set(PROTO_OUTPUT_FILES ${PROTO_HDRS} ${PROTO_SRCS})
set_source_files_properties(${PROTO_OUTPUT_FILES} PROPERTIES GENERATED TRUE)

get_filename_component(PROTO_DIR ${proto_directory}/,
                       DIRECTORY)

set(CODEGEN_HEADERS
    third_party/
    )
set(THIRD_PARTY_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${THIRD_PARTY_INCLUDE})
file(MAKE_DIRECTORY ${root_directory}/releases/include)
file(MAKE_DIRECTORY ${root_directory}/releases/include/codegen/common/)
file(MAKE_DIRECTORY ${root_directory}/releases/include/codegen/third_party/)
file(MAKE_DIRECTORY ${root_directory}/releases/include/codegen/precompile/)
file(MAKE_DIRECTORY ${root_directory}/releases/include/codegen/utils/)
file(MAKE_DIRECTORY ${root_directory}/releases/include/codegen/arrow_compute/ext/)
file(COPY utils/metrics.h DESTINATION ${root_directory}/releases/include/common/)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-attributes")
set(SPARK_COLUMNAR_PLUGIN_SRCS
        jni/JniWrapper.cc
        compute/Backend.cc
        compute/ResultIterator.cc
        config/GlutenConfig.cc
        memory/MemoryAllocator.cc
        memory/ArrowMemoryPool.cc
        ${PROTO_SRCS}
        compute/ProtobufUtils.cc
        operators/c2r/ArrowColumnarToRowConverter.cc
        operators/c2r/ColumnarToRow.cc
        shuffle/reader.cc
        shuffle/ArrowShuffleWriter.cc
        operators/writer/ArrowWriter.cc
        shuffle/Partitioner.cc
        shuffle/FallbackRangePartitioner.cc
        shuffle/HashPartitioner.cc
        shuffle/RoundRobinPartitioner.cc
        shuffle/SinglePartPartitioner.cc
        shuffle/PartitionWriterCreator.cc
        shuffle/LocalPartitionWriter.cc
        shuffle/rss/RemotePartitionWriter.cc
        shuffle/rss/CelebornPartitionWriter.cc
        )

file(MAKE_DIRECTORY ${root_directory}/releases)
add_library(gluten SHARED ${SPARK_COLUMNAR_PLUGIN_SRCS})
add_dependencies(gluten jni_proto)

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=libstdc++fs.a
    RESULT_VARIABLE LIBSTDCXXFS_STATIC_RESULT
    OUTPUT_VARIABLE LIBSTDCXXFS_STATIC_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if (LIBSTDCXXFS_STATIC_RESULT EQUAL 0 AND EXISTS "${LIBSTDCXXFS_STATIC_PATH}")
    message(STATUS "libstdc++fs.a found at: ${LIBSTDCXXFS_STATIC_PATH}")
    target_link_libraries(gluten PRIVATE ${LIBSTDCXXFS_STATIC_PATH})
  else()
    find_library(LIBSTDCXXFS stdc++fs REQUIRED)
    target_link_libraries(gluten PUBLIC ${LIBSTDCXXFS})
  endif()
endif()

find_arrow_lib(${ARROW_LIB_NAME})
find_arrow_lib(${PARQUET_LIB_NAME})

if(ENABLE_HBM)
  include(BuildMemkind)
  target_sources(gluten PRIVATE memory/HbwAllocator.cc)
  target_link_libraries(gluten PRIVATE memkind::memkind)
  add_definitions(-DGLUTEN_ENABLE_HBM)
endif()

if(ENABLE_QAT)
  include(BuildQATzip)
  target_include_directories(gluten PUBLIC ${QATZIP_INCLUDE_DIR})
  target_sources(gluten PRIVATE utils/qat/qat_util.cc)
  target_link_libraries(gluten PUBLIC qatzip::qatzip)
  add_definitions(-DGLUTEN_ENABLE_QAT)
endif()

if(ENABLE_IAA)
  include(BuildQpl)
  target_include_directories(gluten PUBLIC ${QPL_INCLUDE_DIR})
  # set_source_files_properties(utils/qpl/qpl_codec.cc PROPERTIES COMPILE_FLAGS "-mwaitpkg")
  target_sources(gluten PRIVATE utils/qpl/qpl_job_pool.cc utils/qpl/qpl_codec.cc)
  target_link_libraries(gluten PUBLIC qpl::qpl)
  add_definitions(-DGLUTEN_ENABLE_IAA)
endif()

if(BUILD_PROTOBUF)
  build_protobuf()
  message(STATUS "Building ProtoBuf from Source: ${BUILD_PROTOBUF}")
  target_link_libraries(gluten
                        LINK_PRIVATE protobuf::libprotobuf)
else()
  find_protobuf()
  message(STATUS "Use existing ProtoBuf libraries: ${PROTOBUF_LIBRARY}")
  target_link_libraries(gluten
                        LINK_PUBLIC ${PROTOBUF_LIBRARY})
endif()

add_custom_command(OUTPUT ${PROTO_OUTPUT_FILES}
                   COMMAND ${PROTOC_BIN}
                           --proto_path
                           ${proto_directory}/
                           --cpp_out
                           ${PROTO_OUTPUT_DIR}
                           ${PROTO_FILES}
                   DEPENDS  ${PROTO_DIR}
                   COMMENT "Running PROTO compiler"
                   VERBATIM)
add_custom_target(jni_proto ALL DEPENDS ${PROTO_OUTPUT_FILES})
add_dependencies(jni_proto protobuf::libprotobuf)
target_include_directories(gluten PUBLIC ${CMAKE_SYSTEM_INCLUDE_PATH} ${JNI_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_OUTPUT_DIR} ${PROTOBUF_INCLUDE})
set_target_properties(gluten PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY ${root_directory}/releases
)

include(Findjemalloc_pic)
# Build Jemalloc
if(BUILD_JEMALLOC)
  build_jemalloc(${STATIC_JEMALLOC})
  message(STATUS "Building Jemalloc: ${STATIC_JEMALLOC}")
else() #
  find_jemalloc()
  message(STATUS "Use existing Jemalloc libraries")
endif()

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()


if(DEFINED ENV{HADOOP_HOME})
  set(LIBHDFS3_DESTINATION $ENV{HADOOP_HOME}/lib/native)
else()
  set(LIBHDFS3_DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

target_link_libraries(gluten
    PUBLIC Arrow::arrow Arrow::parquet)

install(TARGETS gluten
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/libhdfs.so
        DESTINATION ${LIBHDFS3_DESTINATION})
