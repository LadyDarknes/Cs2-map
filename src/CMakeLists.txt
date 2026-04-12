cmake_minimum_required(VERSION 3.16)
project(Cs2_Map)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Make your project x64!")
endif()

add_definitions(-D_MBCS -D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE)

# Includes
include_directories(.)
include_directories(utils)
include_directories(mapper)

# Sources
set(SOURCES
    "main.cpp"
    "utils/utils.cpp"
    "mapper/intel_driver.cpp"
    "mapper/kdmapper.cpp"
    "mapper/portable_executable.cpp"
    "mapper/service.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Libraries
target_link_libraries(${PROJECT_NAME} 
    version.lib
    kernel32.lib
    user32.lib
    shell32.lib
    advapi32.lib
)

# Build path
set_target_properties(${PROJECT_NAME} PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)