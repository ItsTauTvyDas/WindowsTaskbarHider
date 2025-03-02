cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0115 NEW)

project(WindowsTaskbarHider)

set(CMAKE_CXX_STANDARD       20)
set(CMAKE_CXX_FLAGS          "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++ -pthread")
set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-3")

set(PRODUCT_COMPANY          "ItsTauTvyDas")
set(PRODUCT_URL              "https://github.com/ItsTauTvyDas/WindowsTaskbarHider")
set(PRODUCT_DESCRIPTION      "Windows Taskbar Hider")
set(VERSION_PRODUCT_NUMBER   1)
set(VERSION_PRODUCT_VERSION  0)
set(VERSION_PRODUCT_BUILD    0)
set(VERSION_PRODUCT_REVISION 0)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/language.h" "${lang_h_output}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/language.cpp" "${lang_h_output}")

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

# Create std::wstring array for supported languages
file(GLOB language_files "${CMAKE_SOURCE_DIR}/assets/language/language.*.*.ini")
set(SUPPORTED_LANGUAGES "{")
foreach(file_path ${language_files})
    get_filename_component(filename ${file_path} NAME)
    string(REGEX REPLACE "^language\\.([^.]+)\\.([^.]+)\\.ini$" "\\1" lang_string ${filename})
    string(REGEX REPLACE "^language\\.([^.]+)\\.([^.]+)\\.ini$" "\\2" lang_code ${filename})
    string(APPEND SUPPORTED_LANGUAGES " { L\"${lang_string}\", ${lang_code} },")
endforeach()
string(REGEX REPLACE ",$" "" SUPPORTED_LANGUAGES "${SUPPORTED_LANGUAGES}") # Remove last comma
string(APPEND SUPPORTED_LANGUAGES " }")

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
        # Adding them makes cmake notice changes in language.h.in file, idk how bad it is specifying both files from source and binary directory
        ${CMAKE_BINARY_DIR}/language.h
        ${CMAKE_BINARY_DIR}/language.cpp
        language.h
        language.cpp
        assets/language/language.en.2057.ini
        assets/language/language.uk.1058.ini)

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