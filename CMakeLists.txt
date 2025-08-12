cmake_minimum_required(VERSION 3.20)
project(news_pipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake OPTIONAL)

# Conan packages
find_package(absl CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(XXHASH CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(hiredis REQUIRED)
find_package(RdKafka REQUIRED)       # librdkafka (C) via CMake "RdKafka"
find_package(Protobuf REQUIRED)

# Protobuf (C++)
set(PROTO_SRC_DIR ${CMAKE_SOURCE_DIR}/proto)
set(GEN_CPP_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_CPP_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
  ${PROTO_SRC_DIR}/news.proto
  ${PROTO_SRC_DIR}/scorer.proto
  OUT_VAR PROTO_FILES
  PROTOC_OUT_DIR ${GEN_CPP_DIR})

include_directories(${GEN_CPP_DIR})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${LIBXML2_INCLUDE_DIR})

# Common compile flags
add_compile_options(-Wall -Wextra -Wpedantic)

# ---------- news_gw (RSS/API ingestion) ----------
add_executable(news_gw
  services/gw/src/main.cpp
  services/gw/src/http_fetch.cpp
  services/gw/src/rss_parser.cpp
  services/gw/src/url_norm.cpp
  services/gw/src/dedup.cpp
  services/gw/src/kafka_pub.cpp
  services/gw/src/config.cpp
  ${PROTO_SRCS}
  ${PROTO_HDRS}
)

target_include_directories(news_gw PRIVATE services/gw/include)

target_link_libraries(news_gw
  PRIVATE
    protobuf::libprotobuf
    absl::strings
    fmt::fmt
    nlohmann_json::nlohmann_json
    XXHASH::xxhash
    yaml-cpp::yaml-cpp
    hiredis::hiredis
    RdKafka::rdkafka
    ${CURL_LIBRARIES}
    ${LIBXML2_LIBRARIES}
)

# ---------- news_clean (Phase 2 placeholder) ----------
add_executable(news_clean
  services/clean/src/main.cpp
  ${PROTO_SRCS}
  ${PROTO_HDRS}
)
target_link_libraries(news_clean
  PRIVATE protobuf::libprotobuf fmt::fmt)
