cmake_minimum_required(VERSION 3.21)

set(SERVICE_TARGET          chat_service)
set(SERVICE_VERSION         1)
project(${SERVICE_TARGET}
    VERSION                 ${SERVICE_VERSION}
    LANGUAGES               CXX
)


# Debug, Release, RelWithDebInfo and MinSizeRel
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)


# Устанавливаем версию итогового бинарника в зависимости от ветки в Git
if (NOT GIT_WORKDIR_EMPTY)
    set(GIT_WORKDIR_EMPTY 0 CACHE STRING "Is Git workdir empty" FORCE)
endif()
if (NOT GIT_BRANCH_NAME)
    set(GIT_BRANCH_NAME main CACHE STRING "Git branch name" FORCE)
endif()
string(TOLOWER "${GIT_BRANCH}" GIT_BRANCH_NAME)

# Функция для извлечения абсолютного пути (FOUND_LIB_PATH) для указанной библиотеки (LIB_NAME).
# Применяется, если нужно слинковать статическую библиотеку в обход динамической.
# Используется вот так:
# 1. в переменную LIB_EV_STATIC извлекаем абсолютный путь /usr/lib/libev.a:
#    find_static_library(LIB_EV_STATIC ev)
# 2. линкуем цель со статической библиотекой:
#    target_link_libraries(${BIN_NAME} ${LIB_EV_STATIC})
function(find_static_library FOUND_LIB_PATH LIB_NAME)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    find_library(
        FOUND_${LIB_NAME}_STATIC
        ${LIB_NAME}
        NO_CACHE REQUIRED
    )

    if (FOUND_${LIB_NAME}_STATIC)
        get_filename_component(ABS_FILEPATH ${FOUND_${LIB_NAME}_STATIC} ABSOLUTE)
    else()
        message(SEND_ERROR "Unable to find library ${LIB_NAME}")
    endif()

    set(${FOUND_LIB_PATH} ${ABS_FILEPATH} PARENT_SCOPE)
endfunction()

find_package(gRPC           CONFIG REQUIRED)
find_package(Protobuf       CONFIG REQUIRED)
find_package(Threads        REQUIRED)
find_package(OpenSSL        REQUIRED)
find_package(PostgreSQL     REQUIRED)
find_package(prometheus-cpp REQUIRED)

#
# подготовка для генерации gRPC и Protobuf
#

get_filename_component(PROTO_PATH   "${CMAKE_CURRENT_SOURCE_DIR}/../pb/chat_service.proto" ABSOLUTE)
get_filename_component(PROTO_DIR    "${PROTO_PATH}" DIRECTORY)

set(GENERATED_DIR           "${CMAKE_CURRENT_SOURCE_DIR}/generated")
file(MAKE_DIRECTORY         "${GENERATED_DIR}")

set(PROTO_SRCS              "${GENERATED_DIR}/chat_service.pb.cc")
set(PROTO_HDRS              "${GENERATED_DIR}/chat_service.pb.h")
set(GRPC_SRCS               "${GENERATED_DIR}/chat_service.grpc.pb.cc")
set(GRPC_HDRS               "${GENERATED_DIR}/chat_service.grpc.pb.h")

add_custom_command(
    OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
    COMMAND protobuf::protoc
    ARGS --grpc_out "${GENERATED_DIR}"
         --cpp_out "${GENERATED_DIR}"
         -I "${PROTO_DIR}"
         --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
         "${PROTO_PATH}"
    DEPENDS "${PROTO_PATH}"
)

#
# собираем все исходники в библиотеку
#

file(GLOB                   COMMON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/sources/*/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/sources/*.cpp"
)
file(GLOB                   SERVICE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/sources/*/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sources/*.cpp"
)
set(SERVICE_LIB             service_sources)
add_library(${SERVICE_LIB}  STATIC
    ${COMMON_SOURCES}
    ${SERVICE_SOURCES}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)
set_target_properties(${SERVICE_LIB} PROPERTIES
    LINKER_LANGUAGE         CXX
    CXX_STANDARD            20
    CXX_STANDARD_REQUIRED   ON
    CXX_EXTENSIONS          OFF
)
target_include_directories(${SERVICE_LIB}
    PUBLIC                  "${GENERATED_DIR}"
    PUBLIC                  "${CMAKE_CURRENT_SOURCE_DIR}/../common/include/"
    PUBLIC                  "${CMAKE_CURRENT_SOURCE_DIR}/include/"
)
target_compile_options(${SERVICE_LIB}
    PUBLIC                  -Werror
                            -Wall
                            -Wextra
)
target_compile_definitions(${SERVICE_LIB}
    PUBLIC                  LOGGING_LEVEL_ENABLE_ALL
                            # LOGGING_LEVEL_ENABLE_ALERT
                            # LOGGING_LEVEL_ENABLE_ERROR
                            # LOGGING_LEVEL_ENABLE_WARNG
                            # LOGGING_LEVEL_ENABLE_INFOR
                            # LOGGING_LEVEL_ENABLE_DEBUG
                            # LOGGING_LEVEL_ENABLE_TRACE
)

find_static_library(LIB_BROTLIDEC_STATIC brotlidec)
find_static_library(LIB_BROTLIENC_STATIC brotlienc)
find_static_library(LIB_BROTLICOM_STATIC brotlicommon)
find_static_library(LIB_JSONCPP_STATIC jsoncpp)
find_static_library(LIB_CARES_STATIC cares)
find_static_library(LIB_EV_STATIC ev)
find_static_library(LIB_AMQPCPP_STATIC amqpcpp)
find_static_library(LIB_REACTCPP_STATIC reactcpp)

include(FetchContent)
FetchContent_Declare(
    tntcxx
    GIT_REPOSITORY https://github.com/tarantool/tntcxx.git
)
FetchContent_MakeAvailable(tntcxx)
target_link_libraries(${SERVICE_LIB}
    PUBLIC                  tntcxx::tntcxx
)

target_link_libraries(${SERVICE_LIB}
    PRIVATE                 gRPC::grpc++
                            protobuf::libprotobuf
                            drogon
                            trantor
                            uuid
                            ossp-uuid
                            pqxx
                            pq
                            ${LIB_BROTLIDEC_STATIC}
                            ${LIB_BROTLIENC_STATIC}
                            ${LIB_BROTLICOM_STATIC}
                            ${LIB_JSONCPP_STATIC}
                            ${LIB_CARES_STATIC}
                            ${LIB_AMQPCPP_STATIC}
                            ${LIB_REACTCPP_STATIC}
                            ${LIB_EV_STATIC}
                            prometheus-cpp::core
                            prometheus-cpp::pull
                            OpenSSL::SSL
                            Threads::Threads
)

#
# собираем итоговый бинарник
#
add_executable(${SERVICE_TARGET}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
set_target_properties(${SERVICE_TARGET} PROPERTIES
    LINKER_LANGUAGE         CXX
    CXX_STANDARD            20
    CXX_STANDARD_REQUIRED   ON
    CXX_EXTENSIONS          OFF
)
target_link_libraries(${SERVICE_TARGET}
    PUBLIC                  ${SERVICE_LIB}
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

#
# установка сиервиса и генерация файлов
#

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile.${SERVICE_TARGET}.in"
               "${CMAKE_CURRENT_BINARY_DIR}/Dockerfile.${SERVICE_TARGET}" @ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/make_image.${SERVICE_TARGET}.sh.in"
               "${CMAKE_CURRENT_BINARY_DIR}/make_image.${SERVICE_TARGET}.sh" @ONLY)

include(GNUInstallDirs)
install(TARGETS             ${SERVICE_TARGET}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/Dockerfile.${SERVICE_TARGET}"
        DESTINATION ${CMAKE_INSTALL_BINDIR}/../)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/make_image.${SERVICE_TARGET}.sh"
        DESTINATION ${CMAKE_INSTALL_BINDIR}/../)

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
