cmake_minimum_required(VERSION 3.10)

project(Hud
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

# Try to find local library to link to
# otherwise fetch package dependencies and compile library locally
find_package(SDL3)
if(NOT ${SDL3_FOUND})
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/dependencies/SDL3 EXCLUDE_FROM_ALL)
endif()

find_package(SDL3_ttf)
if(NOT ${SDL3_ttf_FOUND})
	set(SDLTTF_VENDORED ON)
	execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/dependencies/SDL_ttf/external/download.sh)
	add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/dependencies/SDL_ttf EXCLUDE_FROM_ALL)
endif()

add_subdirectory(dependencies/clay)

file(GLOB SRC_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/*.h
    ${CMAKE_CURRENT_LIST_DIR}/src/*.hpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf clay)
target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/dependencies/SDL/include
	${CMAKE_CURRENT_LIST_DIR}/dependencies/SDL_ttf/include
	${CMAKE_CURRENT_LIST_DIR}/dependencies/clay)
