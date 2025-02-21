cmake_minimum_required(VERSION 3.10)
project(WindowsTaskbarHider)
set(CMAKE_CXX_STANDARD 20)
add_executable(WindowsTaskbarHider main.cpp
        utils.cpp
        utils.h
        taskbar.cpp
        taskbar.h
        config.cpp
        config.h
        globals.cpp
        globals.h
        assets/app.rc)
target_link_libraries(WindowsTaskbarHider version)

add_custom_command(
        TARGET WindowsTaskbarHider POST_BUILD
        COMMAND powershell -Command "Write-Host 'Executable size: ' (Get-Item '$<TARGET_FILE:WindowsTaskbarHider>').Length 'bytes'"
)