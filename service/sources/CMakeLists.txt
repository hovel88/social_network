set(SERVICE_SRC_LIB     ${PROJECT_NAME}-src)
set(LIB_SOURCES         ${PROJECT_NAME}-sources)

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

target_link_libraries(${SERVICE_SRC_LIB}
    PUBLIC      pthread
                pqxx
                pq
                crypto
                bcrypt
)

find_package(prometheus-cpp REQUIRED)
target_link_libraries(${SERVICE_SRC_LIB}
    PRIVATE     prometheus-cpp::core
                prometheus-cpp::pull
)
