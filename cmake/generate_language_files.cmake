# Use CMAKE_CURRENT_LIST_DIR to reference the source directory of the script.
set(lang_id_counter 1)
set(lang_h_output "")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../language.h.in" lines)
foreach(line IN LISTS lines)
    string(FIND "${line}" "@LANG_ID_COUNTER@" pos)
    while(pos GREATER -1)
        string(REPLACE "@LANG_ID_COUNTER@" "${lang_id_counter}" line "${line}")
        math(EXPR lang_id_counter "${lang_id_counter} + 1")
        string(FIND "${line}" "@LANG_ID_COUNTER@" pos)
    endwhile()
    set(lang_h_output "${lang_h_output}${line}\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/language.h" "${lang_h_output}")

# Read the generated language.h from the binary directory for mapping.
file(STRINGS "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/language.h" gen_lines)
set(LANGUAGE_MAPPING_ENTRIES "")
foreach(line IN LISTS gen_lines)
    if(line MATCHES "^#define[ \t]+(MSG_[A-Z0-9_]+)[ \t]+[0-9]+")
        string(REGEX REPLACE "^#define[ \t]+(MSG_[A-Z0-9_]+)[ \t]+[0-9]+" "\\1" macro "${line}")
        set(LANGUAGE_MAPPING_ENTRIES "${LANGUAGE_MAPPING_ENTRIES}    { ${macro}, L\"${macro}\" },\n")
    endif()
endforeach()

configure_file("${CMAKE_CURRENT_LIST_DIR}/../language.cpp.in" "${CMAKE_CURRENT_BINARY_DIR_CONFIGURED}/language.cpp" @ONLY)

