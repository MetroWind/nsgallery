cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(Gallery)

include(FetchContent)
FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG v3.1.1
)
FetchContent_MakeAvailable(cxxopts)

set(OATPP_BUILD_TESTS FALSE)
FetchContent_Declare(
  oatpp
  GIT_REPOSITORY https://github.com/oatpp/oatpp.git
)
FetchContent_MakeAvailable(oatpp)

FetchContent_Declare(
  ryml
  GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
  GIT_TAG
  GIT_SHALLOW FALSE  # ensure submodules are checked out
)
FetchContent_MakeAvailable(ryml)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
  inja
  GIT_REPOSITORY https://github.com/pantor/inja.git
  GIT_TAG main
)
include_directories(${json_SOURCE_DIR}/single_include)
set(INJA_USE_EMBEDDED_JSON FALSE)
set(INJA_BUILD_TESTS FALSE)
set(BUILD_BENCHMARK FALSE)
FetchContent_MakeAvailable(inja)
unset(BUILD_BENCHMARK)

set(SOURCE_FILES src/test.cpp)
add_executable(gallery ${SOURCE_FILES})
set_property(TARGET gallery PROPERTY CXX_EXTENSIONS FALSE)
set_property(TARGET gallery PROPERTY CXX_STANDARD 23)

set_property(TARGET gallery PROPERTY COMPILE_WARNING_AS_ERROR TRUE)

target_include_directories(gallery PRIVATE
  ${cxxopts_SOURCE_DIR}/include
  ${inja_SOURCE_DIR}/single_include/inja
)

target_link_libraries(gallery PRIVATE cxxopts oatpp ryml::ryml)
