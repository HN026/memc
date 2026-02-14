cmake_minimum_required(VERSION 3.16)
project(memc
    VERSION   1.0.0
    LANGUAGES CXX
    DESCRIPTION "Linux process memory region data collector"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Library ──────────────────────────────────────────────────────────
add_library(memc_lib STATIC
    src/maps_parser.cpp
    src/smaps_parser.cpp
    src/sampler.cpp
    src/collector.cpp
    src/cli.cpp
    src/process_utils.cpp
)

target_include_directories(memc_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(memc_lib
    PUBLIC  pthread
)

# ── CLI Executable ───────────────────────────────────────────────────
add_executable(memc main.cpp)
target_include_directories(memc
    PRIVATE ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}
)
target_link_libraries(memc PRIVATE memc_lib)

# ── Install ──────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS memc
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS memc_lib
    EXPORT  memcTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/memc
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ── CMake Package Config (find_package support) ──────────────────────
install(EXPORT memcTargets
    FILE        memcTargets.cmake
    NAMESPACE   memc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memc
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/memcConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/memcConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/memcConfig.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/memcConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/memcConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memc
)
