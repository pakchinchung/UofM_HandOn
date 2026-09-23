include("${CMAKE_CURRENT_LIST_DIR}/rule.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/file.cmake")

set(DinoGame_default_library_list )

# Handle files with suffix s, for group default-XC32
if(DinoGame_default_default_XC32_FILE_TYPE_assemble)
add_library(DinoGame_default_default_XC32_assemble OBJECT ${DinoGame_default_default_XC32_FILE_TYPE_assemble})
    DinoGame_default_default_XC32_assemble_rule(DinoGame_default_default_XC32_assemble)
    list(APPEND DinoGame_default_library_list "$<TARGET_OBJECTS:DinoGame_default_default_XC32_assemble>")

endif()

# Handle files with suffix S, for group default-XC32
if(DinoGame_default_default_XC32_FILE_TYPE_assembleWithPreprocess)
add_library(DinoGame_default_default_XC32_assembleWithPreprocess OBJECT ${DinoGame_default_default_XC32_FILE_TYPE_assembleWithPreprocess})
    DinoGame_default_default_XC32_assembleWithPreprocess_rule(DinoGame_default_default_XC32_assembleWithPreprocess)
    list(APPEND DinoGame_default_library_list "$<TARGET_OBJECTS:DinoGame_default_default_XC32_assembleWithPreprocess>")

endif()

# Handle files with suffix [cC], for group default-XC32
if(DinoGame_default_default_XC32_FILE_TYPE_compile)
add_library(DinoGame_default_default_XC32_compile OBJECT ${DinoGame_default_default_XC32_FILE_TYPE_compile})
    DinoGame_default_default_XC32_compile_rule(DinoGame_default_default_XC32_compile)
    list(APPEND DinoGame_default_library_list "$<TARGET_OBJECTS:DinoGame_default_default_XC32_compile>")

endif()

# Handle files with suffix cpp, for group default-XC32
if(DinoGame_default_default_XC32_FILE_TYPE_compile_cpp)
add_library(DinoGame_default_default_XC32_compile_cpp OBJECT ${DinoGame_default_default_XC32_FILE_TYPE_compile_cpp})
    DinoGame_default_default_XC32_compile_cpp_rule(DinoGame_default_default_XC32_compile_cpp)
    list(APPEND DinoGame_default_library_list "$<TARGET_OBJECTS:DinoGame_default_default_XC32_compile_cpp>")

endif()

# Handle files with suffix [cC], for group default-XC32
if(DinoGame_default_default_XC32_FILE_TYPE_dependentObject)
add_library(DinoGame_default_default_XC32_dependentObject OBJECT ${DinoGame_default_default_XC32_FILE_TYPE_dependentObject})
    DinoGame_default_default_XC32_dependentObject_rule(DinoGame_default_default_XC32_dependentObject)
    list(APPEND DinoGame_default_library_list "$<TARGET_OBJECTS:DinoGame_default_default_XC32_dependentObject>")

endif()


# Main target for this project
add_executable(DinoGame_default_image_3WpNnpC2 ${DinoGame_default_library_list})

set_target_properties(DinoGame_default_image_3WpNnpC2 PROPERTIES
    OUTPUT_NAME "default"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${DinoGame_default_output_dir}")
target_link_libraries(DinoGame_default_image_3WpNnpC2 PRIVATE ${DinoGame_default_default_XC32_FILE_TYPE_link})
# Add the link options from the rule file.
DinoGame_default_link_rule( DinoGame_default_image_3WpNnpC2)

# Add bin2hex target for converting built file to a .hex file.
string(REGEX REPLACE [.]elf$ .hex DinoGame_default_image_name_hex ${DinoGame_default_image_name})
add_custom_target(DinoGame_default_Bin2Hex ALL
    COMMAND ${MP_BIN2HEX} \"${DinoGame_default_output_dir}/${DinoGame_default_image_name}\"
    BYPRODUCTS ${DinoGame_default_output_dir}/${DinoGame_default_image_name_hex}
    COMMENT "Convert built file to .hex")
add_dependencies(DinoGame_default_Bin2Hex DinoGame_default_image_3WpNnpC2)




