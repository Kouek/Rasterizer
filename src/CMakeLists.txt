file(GLOB CXX_SRCS "*/*.cpp")

# <lib>
set(TARGET_NAME "rasterizer")
message(STATUS "${TARGET_NAME}.CXX_SRCS: ${CXX_SRCS}")
add_library(
	${TARGET_NAME}
	${CXX_SRCS}
)
# </lib>
