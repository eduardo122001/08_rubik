project( 11_Final_glfw )

message( "Source of application:  ${CMAKE_SOURCE_DIR} " )
message( "Subproject name:  ${PROJECT_NAME} " )

set(CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}/bin")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR} )

# Enable C++11 / C++14 / C++17 / C++20
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_BUILD_TYPE Release )

link_libraries(glfw)

include_directories("${GLFW_SOURCE_DIR}/deps")

if (MATH_LIBRARY)
	message(${MATH_LIBRARY})
    link_libraries("${MATH_LIBRARY}")
endif()

# Workaround for the MS CRT deprecating parts of the standard library
if (MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if (WIN32)
    set(ICON ../glfw.rc)
elseif (APPLE)
    set(ICON ../glfw.icns)
endif()

set(GLAD_GL "${GLFW_SOURCE_DIR}/deps/glad/gl.h")
set(GLAD_GLES2 "${GLFW_SOURCE_DIR}/deps/glad/gles2.h")
set(GETOPT "${GLFW_SOURCE_DIR}/deps/getopt.h"
           "${GLFW_SOURCE_DIR}/deps/getopt.c")
set(TINYCTHREAD "${GLFW_SOURCE_DIR}/deps/tinycthread.h"
                "${GLFW_SOURCE_DIR}/deps/tinycthread.c")

# OpenGL
find_package(OpenGL REQUIRED)

file(GLOB SOURCES "*.cpp" ${DEPENDENCY_DIR}/include/glad/glad/glad.c )
file(GLOB HEADERS "*.h" )
file(GLOB SHADERS "*.vert" "*.frag" "*.vs" "*.fs" )

# ---> NEW: Dynamically grab all solver engine files directly.
# This assumes the 'zz-optimal-core' folder is in the same directory as this CMake file.
file(GLOB ZZ_SOURCES "zz-optimal-core/src/*.cpp")

# ---> NEW: Remove the solver's main functions so they don't collide with your OpenGL main()
list(REMOVE_ITEM ZZ_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/zz-optimal-core/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/zz-optimal-core/src/esp32_main.cpp"
)

# ---> NEW: Add the ZZ_SOURCES directly to your executable
add_executable(
 ${PROJECT_NAME} WIN32 MACOSX_BUNDLE  
 ${HEADERS} ${SOURCES} ${ZZ_SOURCES} ${SHADERS}
 ${ICON} ${GLAD_GL}   )

# ---> NEW: Tell the executable where to find the solver headers
target_include_directories(${PROJECT_NAME} PRIVATE zz-optimal-core/include)
# ---> NEW: Define the macro the solver uses to optimize for desktop architectures
target_compile_definitions(${PROJECT_NAME} PRIVATE DESKTOP_BUILD)

message( "\nBuild mode test is: ${CMAKE_BUILD_TYPE}" )

set(GUI_ONLY_BINARIES  ${PROJECT_NAME})

message( "\nBuild C++ standard is: ${CMAKE_CXX_STANDARD}" ) 

SET(SUBSYSTEM_LINK_FLAGS "-mconsole -mwindows")

# Increase stack size to prevent deep-search stack overflows during FMC
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,--stack,16777216")

target_link_libraries(  ${PROJECT_NAME} 
                        ${SUBSYSTEM_LINK_FLAGS}
                        )