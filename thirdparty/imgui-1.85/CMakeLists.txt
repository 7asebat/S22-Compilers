cmake_minimum_required(VERSION 3.20)

add_library(imgui INTERFACE)

target_sources(imgui INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include/imgui.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/imgui_internal.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_dx11.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_impl_win32.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/imgui.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_demo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_draw.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_tables.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_widgets.cpp
)

target_include_directories(imgui INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(imgui INTERFACE
	d3d11.lib d3dcompiler.lib
)