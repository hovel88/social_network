set(SERVICE_SRC_LIB     ${PROJECT_NAME}-src)
set(LIB_SOURCES         ${PROJECT_NAME}-sources)

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

file(GLOB ${LIB_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_library(${SERVICE_SRC_LIB}
    STATIC      ${${LIB_SOURCES}}
)
add_library(Service::src_lib
    ALIAS       ${SERVICE_SRC_LIB}
)
set_target_properties(${SERVICE_SRC_LIB}
    PROPERTIES  LINKER_LANGUAGE CXX
                CXX_STANDARD 20
                CXX_EXTENSIONS OFF
                CXX_STANDARD_REQUIRED ON
)
target_compile_definitions(${SERVICE_SRC_LIB}
    PRIVATE     LOGGING_LEVEL_ENABLE_ALL
                # LOGGING_LEVEL_ENABLE_ALERT
                # LOGGING_LEVEL_ENABLE_ERROR
                # LOGGING_LEVEL_ENABLE_WARNG
                # LOGGING_LEVEL_ENABLE_INFOR
                # LOGGING_LEVEL_ENABLE_DEBUG
                # LOGGING_LEVEL_ENABLE_TRACE
)

target_compile_options(${SERVICE_SRC_LIB}
    PRIVATE     -Werror
                -Wall
                -Wextra
)

find_static_library(LIB_BROTLIDEC_STATIC brotlidec)
find_static_library(LIB_BROTLIENC_STATIC brotlienc)
find_static_library(LIB_BROTLICOM_STATIC brotlicommon)
find_static_library(LIB_JSONCPP_STATIC jsoncpp)
find_static_library(LIB_CARES_STATIC cares)

target_link_libraries(${SERVICE_SRC_LIB}
    PUBLIC      pthread
                pqxx
                pq
                drogon
                trantor
                ${LIB_BROTLIDEC_STATIC}
                ${LIB_BROTLIENC_STATIC}
                ${LIB_BROTLICOM_STATIC}
                ${LIB_JSONCPP_STATIC}
                ${LIB_CARES_STATIC}
                ossp-uuid
                ssl
                crypto
                bcrypt
)

find_package(prometheus-cpp REQUIRED)
target_link_libraries(${SERVICE_SRC_LIB}
    PRIVATE     prometheus-cpp::core
                prometheus-cpp::pull
)

include(FetchContent)
FetchContent_Declare(
    tntcxx
    GIT_REPOSITORY https://github.com/tarantool/tntcxx.git
)
FetchContent_MakeAvailable(tntcxx)

target_link_libraries(${SERVICE_SRC_LIB}
    PUBLIC      tntcxx::tntcxx
)
