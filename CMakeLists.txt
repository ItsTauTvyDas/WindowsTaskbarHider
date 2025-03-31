cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0115 NEW)

project(WindowsTaskbarHider)

set(CMAKE_CXX_STANDARD             20)
set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -static -static-libgcc -static-libstdc++ -Os -fdata-sections -ffunction-sections")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s -static -lpthread")
set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-3")

set(PRODUCT_COMPANY          "ItsTauTvyDas")
set(PRODUCT_URL              "https://github.com/ItsTauTvyDas/WindowsTaskbarHider")
set(PRODUCT_DESCRIPTION      "Windows Taskbar Hider")
set(VERSION_PRODUCT_NUMBER   1)
set(VERSION_PRODUCT_VERSION  0)
set(VERSION_PRODUCT_BUILD    0)
set(VERSION_PRODUCT_REVISION 0)

# Create std::wstring array for supported languages
file(GLOB language_files "${CMAKE_SOURCE_DIR}/resources/language/language.*.*.ini")
set(DEFAULT_SUPPORTED_LANGUAGES "{")
foreach(file_path ${language_files})
    get_filename_component(filename ${file_path} NAME)
    string(REGEX REPLACE "^language\\.([^.]+)\\.([^.]+)\\.ini$" "\\1" lang_string ${filename})
    string(REGEX REPLACE "^language\\.([^.]+)\\.([^.]+)\\.ini$" "\\2" lang_code ${filename})
    string(APPEND DEFAULT_SUPPORTED_LANGUAGES " { L\"${lang_string}\", ${lang_code} },")
endforeach()
string(REGEX REPLACE ",$" "" DEFAULT_SUPPORTED_LANGUAGES "${DEFAULT_SUPPORTED_LANGUAGES}") # Remove last comma
string(APPEND DEFAULT_SUPPORTED_LANGUAGES " }")

add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/language.h" "${CMAKE_CURRENT_BINARY_DIR}/language.cpp"
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_language_files.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/language.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/language.cpp.in"
        COMMENT "Generating language files (language.h and language.cpp)"
)

configure_file(resources.h.in "${CMAKE_CURRENT_BINARY_DIR}/resources.h" @ONLY)

set(EXECUTABLE_SOURCES
        main.cpp
        utils.cpp
        utils.h
        taskbar.cpp
        taskbar.h
        config.cpp
        config.h
        globals.cpp
        globals.h
        monitors.cpp
        monitors.h
        resources/app.rc
        resources.h
        "${CMAKE_CURRENT_BINARY_DIR}/language.h"
        "${CMAKE_CURRENT_BINARY_DIR}/language.cpp"
        resources/language/language.en.2057.ini
        resources/language/language.uk.1058.ini
)

# Installable version
add_executable(WindowsTaskbarHider WIN32 ${EXECUTABLE_SOURCES})
target_include_directories(WindowsTaskbarHider PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(WindowsTaskbarHider PRIVATE -municode -mwindows)
target_link_options(WindowsTaskbarHider PRIVATE -municode -mwindows)
target_compile_definitions(WindowsTaskbarHider PRIVATE UNICODE _UNICODE IS_PORTABLE=0)
target_link_libraries(WindowsTaskbarHider PRIVATE Dwmapi)

# Portable version
add_executable(WindowsTaskbarHiderPortable WIN32 ${EXECUTABLE_SOURCES})
target_include_directories(WindowsTaskbarHiderPortable PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(WindowsTaskbarHiderPortable PRIVATE -municode -mwindows)
target_link_options(WindowsTaskbarHiderPortable PRIVATE -municode -mwindows)
target_compile_definitions(WindowsTaskbarHiderPortable PRIVATE UNICODE _UNICODE IS_PORTABLE=1)
target_link_libraries(WindowsTaskbarHiderPortable PRIVATE Dwmapi)

# PowerShell command to forcefully kill any existing processes of the application (I was too lazy to close it every time lol)
add_custom_command(
        TARGET WindowsTaskbarHider PRE_BUILD
        COMMAND powershell -NoProfile -Command "Stop-Process -Name WindowsTaskbarHider -Force -ErrorAction SilentlyContinue; exit 0"
)

add_custom_command(
        TARGET WindowsTaskbarHiderPortable PRE_BUILD
        COMMAND powershell -NoProfile -Command "Stop-Process -Name WindowsTaskbarHider -Force -ErrorAction SilentlyContinue; exit 0"
)