cmake_minimum_required(VERSION 3.10)
project(WindowsTaskbarHider)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-static -static-libgcc -static-libstdc++ -pthread")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--whole-archive -lwinpthread -Wl,--no-whole-archive")

add_executable(${PROJECT_NAME} WIN32 main.cpp
        utils.cpp
        utils.h
        taskbar.cpp
        taskbar.h
        config.cpp
        config.h
        globals.cpp
        globals.h
        assets/app.rc
        resources.h.in
        resources.h)

target_link_libraries(${PROJECT_NAME} version)
configure_file(resources.h.in ${CMAKE_BINARY_DIR}/resources.h @ONLY)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND powershell -Command "Write-Host 'Executable size: ' (Get-Item '$<TARGET_FILE:${PROJECT_NAME}>').Length 'bytes'"
)