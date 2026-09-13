cmake_minimum_required(VERSION 3.16)

project(RinICU VERSION 1.0.0 LANGUAGES C CXX)

option(RINICU_BUILD_TESTS "Build RinICU ABI and data-policy tests" ON)
option(RINICU_ENABLE_AUTOSTART
       "Use the RinOS SDK service-manager wrapper before connecting" OFF)
set(RINICU_SDK_INCLUDE_DIR "" CACHE PATH
    "Directory containing the public RinOS-SDK/include/rin headers")

if(NOT RINICU_SDK_INCLUDE_DIR)
    set(RINICU_SDK_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../../RinOS-SDK/include")
endif()
if(NOT EXISTS "${RINICU_SDK_INCLUDE_DIR}/rin/icu_service_abi.h")
    message(FATAL_ERROR
            "RinICU requires RinOS-SDK/include/rin/icu_service_abi.h")
endif()
if(WIN32)
    message(FATAL_ERROR
            "RinICU local-socket transport requires a POSIX/RinOS socket ABI")
endif()

add_library(RinICU STATIC src/client.c)
add_library(RinICU::RinICU ALIAS RinICU)
target_compile_features(RinICU PUBLIC c_std_11)
target_include_directories(RinICU PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${RINICU_SDK_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>)
if(RINICU_ENABLE_AUTOSTART)
    target_compile_definitions(RinICU PRIVATE RIN_ICU_ENABLE_AUTOSTART=1)
endif()
if(NOT MSVC)
    target_compile_options(RinICU PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS RinICU EXPORT RinICUTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT RinICUTargets
        FILE RinICUTargets.cmake
        NAMESPACE RinICU::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RinICU)

configure_file(rinicu.pc.in rinicu.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rinicu.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(RINICU_BUILD_TESTS)
    enable_testing()
    add_executable(rinicu-abi-test tests/abi_contract_test.c)
    target_link_libraries(rinicu-abi-test PRIVATE RinICU::RinICU)
    add_test(NAME rinicu-abi COMMAND rinicu-abi-test)

    add_executable(rinicu-protocol-test tests/protocol_contract_test.c)
    target_link_libraries(rinicu-protocol-test PRIVATE RinICU::RinICU)
    add_test(NAME rinicu-protocol COMMAND rinicu-protocol-test)

    add_executable(rinicu-data-policy-test tests/data_policy_test.c)
    target_link_libraries(rinicu-data-policy-test PRIVATE RinICU::RinICU)
    add_test(NAME rinicu-data-policy COMMAND rinicu-data-policy-test)

    add_executable(rinicu-cpp-abi-test tests/cpp_abi_compile_test.cpp)
    target_link_libraries(rinicu-cpp-abi-test PRIVATE RinICU::RinICU)
    target_compile_features(rinicu-cpp-abi-test PRIVATE cxx_std_17)
    add_test(NAME rinicu-cpp-abi COMMAND rinicu-cpp-abi-test)

    add_executable(rinicu-client-validation-test
                   tests/client_validation_test.c)
    target_include_directories(rinicu-client-validation-test PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/include
                               ${RINICU_SDK_INCLUDE_DIR}
                               ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME rinicu-client-validation COMMAND rinicu-client-validation-test)
endif()
