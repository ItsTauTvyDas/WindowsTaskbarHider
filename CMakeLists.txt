cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0115 NEW)

project(WindowsTaskbarHider)

set(CMAKE_CXX_STANDARD             20)
#set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -static -static-libgcc -static-libstdc++ -Os -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -static -static-libgcc -static-libstdc++ -O2 -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -s -static -pthread")
set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-3")

set(PRODUCT_COMPANY          "ItsTauTvyDas")
set(PRODUCT_URL              "https://github.com/ItsTauTvyDas/WindowsTaskbarHider")
set(PRODUCT_DESCRIPTION      "Windows Taskbar Hider")
set(VERSION_PRODUCT_NUMBER   1)
set(VERSION_PRODUCT_VERSION  0)
set(VERSION_PRODUCT_BUILD    0)
set(VERSION_PRODUCT_REVISION 0)

set(CMAKE_CURRENT_BINARY_DIR_CONFIGURED "${CMAKE_CURRENT_BINARY_DIR}/bin/configured")
set(CMAKE_CURRENT_BINARY_DIR_PORTABLE "${CMAKE_CURRENT_BINARY_DIR}/bin/portable")
set(CMAKE_CURRENT_BINARY_DIR_INSTALLABLE "${CMAKE_CURRENT_BINARY_DIR}/bin/installable")
set(CMAKE_CURRENT_BINARY_DIR_TEST "${CMAKE_CURRENT_BINARY_DIR}/bin/test")

function(generate_guid OUT_VAR input)
    string(MD5 _hash "${input}")
    string(SUBSTRING ${_hash} 0 8  p1)
    string(SUBSTRING ${_hash} 8 4  p2)
    string(SUBSTRING ${_hash} 12 4 p3)
    string(SUBSTRING ${_hash} 16 4 p4)
    string(SUBSTRING ${_hash} 20 12 p5)
    set(${OUT_VAR} "${p1}-${p2}-${p3}-${p4}-${p5}" PARENT_SCOPE)
endfunction()

# Create required folders
file(MAKE_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/bin"
        ${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}
        ${CMAKE_CURRENT_BINARY_DIR_PORTABLE}
        ${CMAKE_CURRENT_BINARY_DIR_TEST}
)

# Create std::wstring array for supported languages
# Clean up the project if you add new message entries
file(GLOB language_files "${CMAKE_SOURCE_DIR}/resources/language/language.*.*.ini")
set(DEFAULT_SUPPORTED_LANGUAGES "{")
foreach(file_path ${language_files})
    get_filename_component(filename ${file_path} NAME)
    string(REGEX REPLACE "^language\\.([^.]+)\\.([^.]+)\\.ini$" "\\1" lang_string ${filename})
    string(REGEX REPLACE "^language\\.([^.]+)\\.([^.]+)\\.ini$" "\\2" lang_code ${filename})
    string(APPEND DEFAULT_SUPPORTED_LANGUAGES " { L\"${lang_string}\", ${lang_code} },")
endforeach()
string(REGEX REPLACE ",$" "" DEFAULT_SUPPORTED_LANGUAGES ${DEFAULT_SUPPORTED_LANGUAGES}) # Remove last comma
string(APPEND DEFAULT_SUPPORTED_LANGUAGES " }")

add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/language.h" "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/language.cpp"
        COMMAND ${CMAKE_COMMAND}
                -DCMAKE_CURRENT_BINARY_DIR_CONFIGURED=${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_language_files.cmake"
        DEPENDS ${language_files}
                "${CMAKE_CURRENT_SOURCE_DIR}/language.h.in"
                "${CMAKE_CURRENT_SOURCE_DIR}/language.cpp.in"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_language_files.cmake"
        COMMENT "Generating language files (language.h and language.cpp) -> ${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}"
)

# Configure resource.h
configure_file(resources.h.in "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/resources.h" @ONLY)

# Executable sources
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
        "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/resources.h"
        taskbar_animation.cpp
        taskbar_animation.h
        "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/language.h"
        "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/language.cpp"
        resources/language/language.en.2057.ini
        resources/language/language.uk.1058.ini
        config_ui.cpp
        config_ui.h
        win_draw.cpp
        win_draw.h
        _compiletime_helper.hpp
)

# Use UCRT consistently (this bundled MinGW's winpthread expects it)
add_compile_options(-mcrtdll=ucrt)
add_link_options(-mcrtdll=ucrt)

# Installable version
add_executable(WindowsTaskbarHider WIN32 ${EXECUTABLE_SOURCES})
target_compile_options(WindowsTaskbarHider PRIVATE -municode -mwindows)
target_link_options(WindowsTaskbarHider PRIVATE -municode -mwindows)
target_compile_definitions(WindowsTaskbarHider PRIVATE UNICODE _UNICODE)
target_link_libraries(WindowsTaskbarHider PRIVATE Dwmapi)
target_include_directories(WindowsTaskbarHider PRIVATE ${CMAKE_CURRENT_BINARY_DIR_CONFIGURED})
set_target_properties(WindowsTaskbarHider PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR_INSTALLABLE})

# Portable version
add_executable(WindowsTaskbarHiderPortable WIN32 ${EXECUTABLE_SOURCES})
target_compile_options(WindowsTaskbarHiderPortable PRIVATE -municode -mwindows)
target_link_options(WindowsTaskbarHiderPortable PRIVATE -municode -mwindows)
target_compile_definitions(WindowsTaskbarHiderPortable PRIVATE UNICODE _UNICODE IS_PORTABLE)
target_link_libraries(WindowsTaskbarHiderPortable PRIVATE Dwmapi)
target_include_directories(WindowsTaskbarHiderPortable PRIVATE ${CMAKE_CURRENT_BINARY_DIR_CONFIGURED})
set_target_properties(WindowsTaskbarHiderPortable PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR_PORTABLE})

# Test (just testing out some related to project or separate code that does not need to be public, this is solely for my own purpose)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_executable(WindowsTaskbarHiderTest WIN32 test/main.cpp)
    target_compile_options(WindowsTaskbarHiderTest PRIVATE -municode -mwindows)
    target_link_options(WindowsTaskbarHiderTest PRIVATE -municode -mwindows)
    target_compile_definitions(WindowsTaskbarHiderTest PRIVATE UNICODE _UNICODE IS_PORTABLE)
    target_link_libraries(WindowsTaskbarHiderTest PRIVATE Dwmapi)
    target_include_directories(WindowsTaskbarHiderTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR_CONFIGURED})
    set_target_properties(WindowsTaskbarHiderTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR_TEST})
endif()

# PowerShell command to forcefully kill any existing processes of the application (I was too lazy to close it every time lol)
add_custom_command(
        TARGET WindowsTaskbarHider PRE_BUILD
        COMMAND powershell -NoProfile -Command "Stop-Process -Name WindowsTaskbarHider -Force -ErrorAction SilentlyContinue; exit 0"
)

add_custom_command(
        TARGET WindowsTaskbarHiderPortable PRE_BUILD
        COMMAND powershell -NoProfile -Command "Stop-Process -Name WindowsTaskbarHider -Force -ErrorAction SilentlyContinue; exit 0"
)

# Installer stuff (requires WixTools to be installed)
# https://github.com/wixtoolset/wix3
# https://cmake.org/cmake/help/v3.9/module/CPackWIX.html
# https://cmake.org/cmake/help/latest/module/CPack.html
set(CPACK_GENERATOR "WIX")
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/ItsTauTvyDas/WindowsTaskbarHider")
set(CPACK_WIX_PROPERTY_ARPCOMMENTS "Hide taskbar to make your wallpaper more clear!")
set(CPACK_PACKAGE_VERSION "${VERSION_PRODUCT_NUMBER}.${VERSION_PRODUCT_VERSION}.${VERSION_PRODUCT_BUILD}.${VERSION_PRODUCT_REVISION}")
generate_guid(CPACK_WIX_PRODUCT_GUID ${PROJECT_NAME})
generate_guid(CPACK_WIX_UPGRADE_GUID ${CPACK_PACKAGE_VERSION})
set(CPACK_PACKAGE_VENDOR "By ItsTauTvyDas")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})
set(CPACK_PACKAGE_EXECUTABLES ${PROJECT_NAME} ${PRODUCT_DESCRIPTION})
set(CPACK_WIX_PROGRAM_MENU_FOLDER ${PROJECT_NAME})
set(CPACK_CREATE_DESKTOP_LINKS ${PROJECT_NAME})
install(TARGETS WindowsTaskbarHider RUNTIME DESTINATION .)
include(CPack)
add_custom_target(
        WindowsTaskbarHiderInstaller
        COMMAND powershell -NoProfile -Command "cpack.exe -G WIX --config ${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake -VV --debug --trace-expand | Tee-Object -FilePath ${PROJECT_NAME}.msi.log"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR_INSTALLABLE}
        COMMENT "CPACK"
        VERBATIM
        DEPENDS WindowsTaskbarHider
)