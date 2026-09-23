include("${CMAKE_CURRENT_LIST_DIR}/rule.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/file.cmake")

set(TestPl10Again_default_library_list )

# Handle files with suffix s, for group default-XC32
if(TestPl10Again_default_default_XC32_FILE_TYPE_assemble)
add_library(TestPl10Again_default_default_XC32_assemble OBJECT ${TestPl10Again_default_default_XC32_FILE_TYPE_assemble})
    TestPl10Again_default_default_XC32_assemble_rule(TestPl10Again_default_default_XC32_assemble)
    list(APPEND TestPl10Again_default_library_list "$<TARGET_OBJECTS:TestPl10Again_default_default_XC32_assemble>")

endif()

# Handle files with suffix S, for group default-XC32
if(TestPl10Again_default_default_XC32_FILE_TYPE_assembleWithPreprocess)
add_library(TestPl10Again_default_default_XC32_assembleWithPreprocess OBJECT ${TestPl10Again_default_default_XC32_FILE_TYPE_assembleWithPreprocess})
    TestPl10Again_default_default_XC32_assembleWithPreprocess_rule(TestPl10Again_default_default_XC32_assembleWithPreprocess)
    list(APPEND TestPl10Again_default_library_list "$<TARGET_OBJECTS:TestPl10Again_default_default_XC32_assembleWithPreprocess>")

endif()

# Handle files with suffix [cC], for group default-XC32
if(TestPl10Again_default_default_XC32_FILE_TYPE_compile)
add_library(TestPl10Again_default_default_XC32_compile OBJECT ${TestPl10Again_default_default_XC32_FILE_TYPE_compile})
    TestPl10Again_default_default_XC32_compile_rule(TestPl10Again_default_default_XC32_compile)
    list(APPEND TestPl10Again_default_library_list "$<TARGET_OBJECTS:TestPl10Again_default_default_XC32_compile>")

endif()

# Handle files with suffix cpp, for group default-XC32
if(TestPl10Again_default_default_XC32_FILE_TYPE_compile_cpp)
add_library(TestPl10Again_default_default_XC32_compile_cpp OBJECT ${TestPl10Again_default_default_XC32_FILE_TYPE_compile_cpp})
    TestPl10Again_default_default_XC32_compile_cpp_rule(TestPl10Again_default_default_XC32_compile_cpp)
    list(APPEND TestPl10Again_default_library_list "$<TARGET_OBJECTS:TestPl10Again_default_default_XC32_compile_cpp>")

endif()

# Handle files with suffix [cC], for group default-XC32
if(TestPl10Again_default_default_XC32_FILE_TYPE_dependentObject)
add_library(TestPl10Again_default_default_XC32_dependentObject OBJECT ${TestPl10Again_default_default_XC32_FILE_TYPE_dependentObject})
    TestPl10Again_default_default_XC32_dependentObject_rule(TestPl10Again_default_default_XC32_dependentObject)
    list(APPEND TestPl10Again_default_library_list "$<TARGET_OBJECTS:TestPl10Again_default_default_XC32_dependentObject>")

endif()


# Main target for this project
add_executable(TestPl10Again_default_image_dH5c545c ${TestPl10Again_default_library_list})

set_target_properties(TestPl10Again_default_image_dH5c545c PROPERTIES
    OUTPUT_NAME "default"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${TestPl10Again_default_output_dir}")
target_link_libraries(TestPl10Again_default_image_dH5c545c PRIVATE ${TestPl10Again_default_default_XC32_FILE_TYPE_link})
# Add the link options from the rule file.
TestPl10Again_default_link_rule( TestPl10Again_default_image_dH5c545c)

# Add bin2hex target for converting built file to a .hex file.
string(REGEX REPLACE [.]elf$ .hex TestPl10Again_default_image_name_hex ${TestPl10Again_default_image_name})
add_custom_target(TestPl10Again_default_Bin2Hex ALL
    COMMAND ${MP_BIN2HEX} \"${TestPl10Again_default_output_dir}/${TestPl10Again_default_image_name}\"
    BYPRODUCTS ${TestPl10Again_default_output_dir}/${TestPl10Again_default_image_name_hex}
    COMMENT "Convert built file to .hex")
add_dependencies(TestPl10Again_default_Bin2Hex TestPl10Again_default_image_dH5c545c)




