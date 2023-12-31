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

# FetchContent_Declare(
#   oatpp
#   GIT_REPOSITORY https://github.com/oatpp/oatpp.git
# )
# set(OATPP_BUILD_TESTS FALSE)
# FetchContent_MakeAvailable(oatpp)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

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

add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=16 )
add_definitions( -DMAGICKCORE_HDRI_ENABLE=0 )
find_package(ImageMagick COMPONENTS Magick++ REQUIRED)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

set(SOURCE_FILES
  src/app.cpp
  src/app.hpp
  src/config.cpp
  src/config.hpp
  src/file_cache.hpp
  src/image_source.cpp
  src/image_source.hpp
  src/main.cpp
  src/metadata.cpp
  src/metadata.hpp
  src/representation.cpp
  src/representation.hpp
  src/utils.hpp
)
add_executable(gallery ${SOURCE_FILES})
set_property(TARGET gallery PROPERTY CXX_EXTENSIONS FALSE)
set_property(TARGET gallery PROPERTY CXX_STANDARD 23)

set_property(TARGET gallery PROPERTY COMPILE_WARNING_AS_ERROR TRUE)
target_compile_options(gallery PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(gallery PRIVATE
  ${cxxopts_SOURCE_DIR}/include
  ${inja_SOURCE_DIR}/single_include/inja
  ${ImageMagick_INCLUDE_DIRS}
)

target_link_libraries(gallery PRIVATE
  cxxopts
  httplib
  spdlog
  ryml::ryml
  ${ImageMagick_LIBRARIES}
)
