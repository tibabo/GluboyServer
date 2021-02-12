cmake_minimum_required(VERSION 3.10)

add_subdirectory(libs/websocketpp)

# set the project name
project(GluboyServer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GL3W)

file(GLOB SOURCES 
	./src/*.cpp
	./libs/imgui/*.cpp
	./libs/imgui/examples/libs/gl3w/GL/gl3w.c
	./libs/imgui/backends/imgui_impl_glfw.cpp
	./libs/imgui/backends/imgui_impl_opengl3.cpp
)


# Use folders for nice tree in Visual Studio and XCode
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(WIN32)
	set(CORE_MSVC_USE_COMMON_OPTIONS off)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	set(GLFW_LIBRARY glfw3 opengl32)

else()
	if(APPLE)
		set(CMAKE_C_FLAGS "-x objective-c")
		find_library(GL_LIBRARY OpenGL)
		find_library(COCOA_LIBRARY Cocoa)
		find_library(IOKIT_LIBRARY IOKit)
		find_library(CORE_VIDEO_LIBRARY CoreVideo)
		set(GLFW_LIBRARY glfw3)
	else()
		set(GLFW_LIBRARY glfw GL)
	endif()
endif()

# add the executable
add_executable(GluboyServer ${SOURCES} ${PLATFORM_SOURCES})

TARGET_LINK_LIBRARIES(GluboyServer 
						${COCOA_LIBRARY} 
						${GL_LIBRARY} 
						${IOKIT_LIBRARY} 
						${CORE_VIDEO_LIBRARY} 
						${GLFW_LIBRARY}
						${GTK3_LIBRARIES})

target_include_directories(GluboyServer PUBLIC
        "${PROJECT_SOURCE_DIR}/libs/"
		"${PROJECT_SOURCE_DIR}/libs/imgui/examples/libs/gl3w"
        "${PROJECT_SOURCE_DIR}/libs/imgui"
		"${PROJECT_SOURCE_DIR}/libs/imgui/backends"
		"${PROJECT_SOURCE_DIR}/libs/websocketpp"
        )
        
if(WIN32)
#	target_link_options(GluboyServer PUBLIC "LINKER:/subsystem:windows,/entry:mainCRTStartup")
	target_link_directories(GluboyServer PUBLIC
		"${PROJECT_SOURCE_DIR}/libs/imgui/examples/libs/glfw/lib-vc2010-64/"
		"${PROJECT_SOURCE_DIR}/../boost_1_75_0/stage/x64/lib/")
	target_include_directories(GluboyServer PUBLIC
        "${PROJECT_SOURCE_DIR}/libs/imgui/examples/libs/glfw/include"
		"${PROJECT_SOURCE_DIR}/../boost_1_75_0/"
        )

endif()

if(APPLE)
	target_link_directories(GluboyServer PUBLIC
		"${PROJECT_SOURCE_DIR}/libs/glfw/lib-macos/")
	target_include_directories(GluboyServer PUBLIC
        "${PROJECT_SOURCE_DIR}/libs/glfw/include"
        )

endif()
