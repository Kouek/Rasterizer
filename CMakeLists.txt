cmake_minimum_required(VERSION 3.18)

project("Rasterizer" LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/3rd")
set(INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include")

configure_file(
	"${INCLUDE_DIR}/cmake_in"
	"${INCLUDE_DIR}/cmake_in.h"
)

# <dependencies>
add_subdirectory("${THIRD_PARTY_DIR}/glfw")
# </dependencies>

# <global_includes>
include_directories(${THIRD_PARTY_DIR})
include_directories("${THIRD_PARTY_DIR}/glad/include")
include_directories("${THIRD_PARTY_DIR}/glm")
include_directories(${INCLUDE_DIR})
# </global_includes>

add_subdirectory("src")

# <app>
set(TARGET_NAME "demo")
add_executable(
	${TARGET_NAME}
	"main.cpp"
	"${THIRD_PARTY_DIR}/glad/src/glad.c"
)
target_link_libraries(
	${TARGET_NAME}
	PRIVATE
	"rasterizer"
	"glfw"
)
# </app>
