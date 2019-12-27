cmake_minimum_required(VERSION 3.12)
project(PDG VERSION 0.1.0 LANGUAGES CXX)

find_package(LLVM REQUIRED CONFIG)
find_package(svf REQUIRED COMPONENTS Svf)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/PDG)
add_definitions(${LLVM_DEFINITIONS})

add_library(pdg MODULE
        lib/PDG/PDG.cpp
        lib/PDG/PDGBuilder.cpp
        lib/PDG/PDGLLVMNode.cpp
        lib/PDG/LLVMMemorySSADefUseAnalysisResults.cpp
        lib/PDG/LLVMDominanceTree.cpp
        lib/PDG/SVFGDefUseAnalysisResults.cpp
        lib/PDG/IndirectCallSitesAnalysis.cpp
        lib/PDG/SVFGIndirectCallSiteResults.cpp
        lib/Passes/PDGBuildPasses.cpp
        lib/Debug/PDGPrinter.cpp
        lib/Debug/CallSiteConnections.cpp
        lib/Debug/SVFGTraversal.cpp
)

target_include_directories(pdg PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LLVM_INCLUDE_DIRS}
        ${svf_INCLUDE_DIRS}
)

target_link_libraries(pdg PRIVATE
                      svf::Svf
)

if ($ENV{CLION_IDE})
    include_directories("/usr/local/include/llvm/")
    include_directories("/usr/local/include/llvm-c/")
endif ()

target_compile_features(pdg PRIVATE cxx_range_for cxx_auto_type cxx_std_14)
target_compile_options(pdg PRIVATE -fno-rtti -g)

install(TARGETS pdg
        EXPORT pdgTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT pdg)
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PDG
        COMPONENT pdg)

install(
        EXPORT pdgTargets
        NAMESPACE pdg::
        DESTINATION ${INSTALL_CONFIGDIR}
        COMPONENT pdg
)
export(
        EXPORT pdgTargets
        NAMESPACE pdg::
        FILE ${CMAKE_CURRENT_BINARY_DIR}/pdgTargets.cmake
)

export(PACKAGE pdg)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/pdgConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/pdgConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/pdgConfig.cmake
        INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/pdgConfigVersion.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/pdgConfig.cmake
        DESTINATION ${INSTALL_CONFIGDIR}
        )
export(PACKAGE pdg)


