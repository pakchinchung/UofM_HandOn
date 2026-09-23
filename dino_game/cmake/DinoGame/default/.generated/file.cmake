# The following variables contains the files used by the different stages of the build process.
set(DinoGame_default_default_XC32_FILE_TYPE_assemble)
set_source_files_properties(${DinoGame_default_default_XC32_FILE_TYPE_assemble} PROPERTIES LANGUAGE ASM)

# For assembly files, add "." to the include path for each file so that .include with a relative path works
foreach(source_file ${DinoGame_default_default_XC32_FILE_TYPE_assemble})
        set_source_files_properties(${source_file} PROPERTIES INCLUDE_DIRECTORIES "$<PATH:NORMAL_PATH,$<PATH:REMOVE_FILENAME,${source_file}>>")
endforeach()

set(DinoGame_default_default_XC32_FILE_TYPE_assembleWithPreprocess)
set_source_files_properties(${DinoGame_default_default_XC32_FILE_TYPE_assembleWithPreprocess} PROPERTIES LANGUAGE ASM)

# For assembly files, add "." to the include path for each file so that .include with a relative path works
foreach(source_file ${DinoGame_default_default_XC32_FILE_TYPE_assembleWithPreprocess})
        set_source_files_properties(${source_file} PROPERTIES INCLUDE_DIRECTORIES "$<PATH:NORMAL_PATH,$<PATH:REMOVE_FILENAME,${source_file}>>")
endforeach()

set(DinoGame_default_default_XC32_FILE_TYPE_compile
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/exceptions.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/initialization.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/interrupts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/libc_syscalls.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/peripheral/clock/plib_clock.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/peripheral/evsys/plib_evsys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/peripheral/nvic/plib_nvic.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/peripheral/port/plib_port.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/startup_xc32.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/stdio/xc32_monitor.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/breakout.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/bringup.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/dino_game.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/gfx_assets.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/i2c_bus.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/led.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/mcp23008.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/millis.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/pot.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/ssd1306.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/uart.c")
set_source_files_properties(${DinoGame_default_default_XC32_FILE_TYPE_compile} PROPERTIES LANGUAGE C)
set(DinoGame_default_default_XC32_FILE_TYPE_compile_cpp)
set_source_files_properties(${DinoGame_default_default_XC32_FILE_TYPE_compile_cpp} PROPERTIES LANGUAGE CXX)
set(DinoGame_default_default_XC32_FILE_TYPE_link)

# The linker script used for the build.
set(DinoGame_default_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default/PIC32CM6408PL10048.ld")
set(DinoGame_default_image_name "default.elf")
set(DinoGame_default_image_base_name "default")

# The output directory of the final image.
set(DinoGame_default_output_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../../out/DinoGame")

# The full path to the final image.
set(DinoGame_default_full_path_to_image ${DinoGame_default_output_dir}/${DinoGame_default_image_name})
