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

# Some weird bug, where ninja can complain about language.cpp with an error "missing and no known rule to make it"
# After it first fails, if you change something in this file (just make a change, like editing or adding a comment somewhere, just to trigger a change), then it succeeds
# Pre-creating the files fixes the issue
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/language.h" "${lang_h_output}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/language.cpp" "${lang_h_output}")

# Add language files (cmake for some reason doesn't see changes in file, adding them forcefully)
#file(GLOB LANG_INI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/language/language.*.ini")
#set(ALL_LANG_INI_OUTPUTS "")
#foreach(ini_file IN LISTS LANG_INI_FILES)
#    get_filename_component(ini_filename ${ini_file} NAME)
#    set(output_file "${CMAKE_BINARY_DIR}/assets/language/${ini_filename}")
#    add_custom_command(
#            OUTPUT ${output_file}
#            COMMAND ${CMAKE_COMMAND} -E copy ${ini_file} ${output_file}
#            DEPENDS ${ini_file}
#            COMMENT "Copying ${ini_file} to ${output_file}"
#    )
#    list(APPEND ALL_LANG_INI_OUTPUTS ${output_file})
#endforeach()
#add_custom_target(copy_language_inis ALL
#        DEPENDS ${ALL_LANG_INI_OUTPUTS}
#)

# Replace all @LANG_ID_COUNTER@ in language.h.in
set(lang_id_counter 1)
set(lang_h_output "")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/language.h.in" lines)
foreach(line IN LISTS lines)
    string(FIND "${line}" "@LANG_ID_COUNTER@" pos)
    while(pos GREATER -1)
        string(REPLACE "@LANG_ID_COUNTER@" "${lang_id_counter}" line "${line}")
        math(EXPR lang_id_counter "${lang_id_counter} + 1")
        string(FIND "${line}" "@LANG_ID_COUNTER@" pos)
    endwhile()
    set(lang_h_output "${lang_h_output}${line}\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/language.h" "${lang_h_output}")

# Map language.h into unordered_map variable in language.cpp.inj
file(STRINGS "${CMAKE_CURRENT_BINARY_DIR}/language.h" gen_lines)
set(LANGUAGE_MAPPING_ENTRIES "")
foreach(line IN LISTS gen_lines)
    if(line MATCHES "^#define[ \t]+(MSG_[A-Z0-9_]+)[ \t]+[0-9]+")
        string(REGEX REPLACE "^#define[ \t]+(MSG_[A-Z0-9_]+)[ \t]+[0-9]+" "\\1" macro "${line}")
        set(LANGUAGE_MAPPING_ENTRIES "${LANGUAGE_MAPPING_ENTRIES}    { ${macro}, L\"${macro}\" },\n")
    endif()
endforeach()

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
        resources.h
        language.h
        language.cpp
        assets/language/language.en.ini
        assets/language/language.uk.ini)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/language.cpp.in ${CMAKE_BINARY_DIR}/language.cpp @ONLY)
configure_file(resources.h.in ${CMAKE_BINARY_DIR}/resources.h @ONLY)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

target_compile_options(${PROJECT_NAME} PRIVATE -municode -mwindows)
target_link_options(${PROJECT_NAME} PRIVATE -municode -mwindows)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND powershell -Command "Write-Host 'Executable size: ' (Get-Item '$<TARGET_FILE:${PROJECT_NAME}>').Length 'bytes'"
)

add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND powershell -NoProfile -Command "Stop-Process -Name ${PROJECT_NAME} -Force -ErrorAction SilentlyContinue; exit 0"
)