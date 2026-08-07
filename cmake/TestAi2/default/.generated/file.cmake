# The following variables contains the files used by the different stages of the build process.
set(TestAi2_default_default_XC8_FILE_TYPE_assemble)
set_source_files_properties(${TestAi2_default_default_XC8_FILE_TYPE_assemble} PROPERTIES LANGUAGE ASM)

# For assembly files, add "." to the include path for each file so that .include with a relative path works
foreach(source_file ${TestAi2_default_default_XC8_FILE_TYPE_assemble})
        set_source_files_properties(${source_file} PROPERTIES INCLUDE_DIRECTORIES "$<PATH:NORMAL_PATH,$<PATH:REMOVE_FILENAME,${source_file}>>")
endforeach()

set(TestAi2_default_default_XC8_FILE_TYPE_assemblePreprocess "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/system/src/protected_io.S")
set_source_files_properties(${TestAi2_default_default_XC8_FILE_TYPE_assemblePreprocess} PROPERTIES LANGUAGE ASM)

# For assembly files, add "." to the include path for each file so that .include with a relative path works
foreach(source_file ${TestAi2_default_default_XC8_FILE_TYPE_assemblePreprocess})
        set_source_files_properties(${source_file} PROPERTIES INCLUDE_DIRECTORIES "$<PATH:NORMAL_PATH,$<PATH:REMOVE_FILENAME,${source_file}>>")
endforeach()

set(TestAi2_default_default_XC8_FILE_TYPE_compile
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/breakout.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/bringup.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/dino_game.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/gfx_assets.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/i2c_bus.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/adc/src/adc0.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/i2c_host/src/twi0.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/system/src/clock.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/system/src/config_bits.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/system/src/interrupt.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/system/src/pins.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/system/src/system.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/timer/src/tca0.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/uart/src/usart1.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcc_generated_files/vref/src/vref.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/mcp23008.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/millis.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/pot.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../config.mcc/ssd1306.c")
set_source_files_properties(${TestAi2_default_default_XC8_FILE_TYPE_compile} PROPERTIES LANGUAGE C)
set(TestAi2_default_default_XC8_FILE_TYPE_link)
set(TestAi2_default_default_XC8_FILE_TYPE_objcopy_avr)
set(TestAi2_default_image_name "default.elf")
set(TestAi2_default_image_base_name "default")

# The output directory of the final image.
set(TestAi2_default_output_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../../out/TestAi2")

# The full path to the final image.
set(TestAi2_default_full_path_to_image ${TestAi2_default_output_dir}/${TestAi2_default_image_name})

# Potential output file extensions
set(output_extensions
    .hex
    .hxl
    .mum
    .o
    .sdb
    .sym
    .cmf)
list(TRANSFORM output_extensions PREPEND "${TestAi2_default_output_dir}/${TestAi2_default_image_base_name}")
