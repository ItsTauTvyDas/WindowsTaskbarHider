cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0115 NEW)

project(WindowsTaskbarHider)

set(CMAKE_CXX_STANDARD       20)
set(CMAKE_CXX_FLAGS          "-static -static-libgcc -static-libstdc++ -pthread")

set(PRODUCT_COMPANY          "ItsTauTvyDas")
set(PRODUCT_URL              "https://github.com/ItsTauTvyDas/WindowsTaskbarHider")
set(PRODUCT_DESCRIPTION      "Windows Taskbar Hider")
set(VERSION_PRODUCT_NUMBER   1)
set(VERSION_PRODUCT_VERSION  0)
set(VERSION_PRODUCT_BUILD    0)
set(VERSION_PRODUCT_REVISION 0)

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
        language/language.en.rc
        resources.h
        stdcerr.h
        language.h)

file(REMOVE ${CMAKE_BINARY_DIR}/resources.h)
configure_file(resources.h.in ${CMAKE_BINARY_DIR}/resources.h @ONLY)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

if(MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE -municode -mwindows)
    target_link_options(${PROJECT_NAME} PRIVATE -municode -mwindows)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND powershell -Command "Write-Host 'Executable size: ' (Get-Item '$<TARGET_FILE:${PROJECT_NAME}>').Length 'bytes'"
)

add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND powershell -NoProfile -Command "Stop-Process -Name ${PROJECT_NAME} -Force -ErrorAction SilentlyContinue; exit 0"
)