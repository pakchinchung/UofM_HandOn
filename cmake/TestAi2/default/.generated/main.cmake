include("${CMAKE_CURRENT_LIST_DIR}/rule.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/file.cmake")

set(TestAi2_default_library_list )

# Handle files with suffix (s|as|asm|AS|ASM|As|aS|Asm), for group default-XC8
if(TestAi2_default_default_XC8_FILE_TYPE_assemble)
add_library(TestAi2_default_default_XC8_assemble OBJECT ${TestAi2_default_default_XC8_FILE_TYPE_assemble})
    TestAi2_default_default_XC8_assemble_rule(TestAi2_default_default_XC8_assemble)
    list(APPEND TestAi2_default_library_list "$<TARGET_OBJECTS:TestAi2_default_default_XC8_assemble>")

endif()

# Handle files with suffix S, for group default-XC8
if(TestAi2_default_default_XC8_FILE_TYPE_assemblePreprocess)
add_library(TestAi2_default_default_XC8_assemblePreprocess OBJECT ${TestAi2_default_default_XC8_FILE_TYPE_assemblePreprocess})
    TestAi2_default_default_XC8_assemblePreprocess_rule(TestAi2_default_default_XC8_assemblePreprocess)
    list(APPEND TestAi2_default_library_list "$<TARGET_OBJECTS:TestAi2_default_default_XC8_assemblePreprocess>")

endif()

# Handle files with suffix [cC], for group default-XC8
if(TestAi2_default_default_XC8_FILE_TYPE_compile)
add_library(TestAi2_default_default_XC8_compile OBJECT ${TestAi2_default_default_XC8_FILE_TYPE_compile})
    TestAi2_default_default_XC8_compile_rule(TestAi2_default_default_XC8_compile)
    list(APPEND TestAi2_default_library_list "$<TARGET_OBJECTS:TestAi2_default_default_XC8_compile>")

endif()

# Handle files with suffix elf, for group default-XC8
if(TestAi2_default_default_XC8_FILE_TYPE_objcopy_avr)
add_library(TestAi2_default_default_XC8_objcopy_avr OBJECT ${TestAi2_default_default_XC8_FILE_TYPE_objcopy_avr})
    TestAi2_default_default_XC8_objcopy_avr_rule(TestAi2_default_default_XC8_objcopy_avr)
    list(APPEND TestAi2_default_library_list "$<TARGET_OBJECTS:TestAi2_default_default_XC8_objcopy_avr>")

endif()


# Main target for this project
add_executable(TestAi2_default_image_dRgdzhGG ${TestAi2_default_library_list})

set_target_properties(TestAi2_default_image_dRgdzhGG PROPERTIES
    OUTPUT_NAME "default"
    SUFFIX ".elf"
    ADDITIONAL_CLEAN_FILES "${output_extensions}"
    RUNTIME_OUTPUT_DIRECTORY "${TestAi2_default_output_dir}")
target_link_libraries(TestAi2_default_image_dRgdzhGG PRIVATE ${TestAi2_default_default_XC8_FILE_TYPE_link})

#Add objcopy steps
TestAi2_default_objcopy_avr_rule(TestAi2_default_image_dRgdzhGG)
# Add the link options from the rule file.
TestAi2_default_link_rule( TestAi2_default_image_dRgdzhGG)


