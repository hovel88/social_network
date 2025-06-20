cmake_minimum_required(VERSION 3.21)

set(SERVICE_TARGET      social_network)
project(${SERVICE_TARGET}
    VERSION     1.0.0
    LANGUAGES   CXX
)

# Debug, Release, RelWithDebInfo and MinSizeRel
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)

include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/include/)
add_subdirectory(sources)

add_executable(${SERVICE_TARGET}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
target_link_libraries(${SERVICE_TARGET}
    PUBLIC      Service::src_lib
)

if (BUILD_TYPE STREQUAL RELEASE)
    find_program(STRIP_EXECUTABLE strip)
    add_custom_target(strip
        ALL
        COMMAND     ${STRIP_EXECUTABLE} ${SERVICE_TARGET}
        DEPENDS     ${SERVICE_TARGET}
        COMMENT     "Build type is ${CMAKE_BUILD_TYPE}, stripping target binary file"
    )
endif()

message(STATUS "Predefined build flags for build types")
message(STATUS "C++ flags, Debug configuration                   : ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "C++ flags, Release configuration                 : ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "C++ flags, Release with Debug info configuration : ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "C++ flags, minimal Release configuration         : ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message(STATUS "C flags, Debug configuration                     : ${CMAKE_C_FLAGS_DEBUG}")
message(STATUS "C flags, Release configuration                   : ${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "C flags, Release with Debug info configuration   : ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
message(STATUS "C flags, minimal Release configuration           : ${CMAKE_C_FLAGS_MINSIZEREL}")
message(STATUS " ")
message(STATUS "Build type            : ${CMAKE_BUILD_TYPE}")
message(STATUS "Build with C++ flags  : ${CMAKE_CXX_FLAGS_${BUILD_TYPE}} ${CMAKE_CXX_FLAGS}")
message(STATUS "Build with C flags    : ${CMAKE_C_FLAGS_${BUILD_TYPE}} ${CMAKE_C_FLAGS}")

include(GNUInstallDirs)
install(TARGETS ${SERVICE_TARGET}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
