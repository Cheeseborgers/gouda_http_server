cmake_minimum_required(VERSION 3.31)
project(gouda_http_server)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(USE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)

    if(CCACHE_PROGRAM)
        message("Found ccache at: ${CCACHE_PROGRAM}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    else()
        message(WARNING "ccache not found, skipping ccache usage.")
    endif()
endif()

find_package(nlohmann_json REQUIRED)

add_executable(${PROJECT_NAME}
        routes.hpp
        include/file_cache.hpp
        src/file_cache.cpp
        include/sockets/base_socket.hpp
        src/sockets/base_socket.cpp
        "include/sockets/client_socket.hpp"
        include/sockets/server_socket.hpp
        src/sockets/client_socket.cpp
        src/sockets/server_socket.cpp
        include/sockets/accepted_socket.hpp)

target_sources(${PROJECT_NAME}
        PRIVATE
        main.cpp
        src/http_server.cpp
        src/socket_wrapper.cpp
        src/client_handler.cpp
        src/thread_pool.cpp
        src/logger.cpp
        src/http_request_parser.cpp
        src/router.cpp
        src/http_response_builder.cpp
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
