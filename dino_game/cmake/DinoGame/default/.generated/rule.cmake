# The following functions contains all the flags passed to the different build stages.

set(PACK_REPO_PATH "C:/Users/A76570/.mchp_packs" CACHE PATH "Path to the root of a pack repository.")

function(DinoGame_default_default_XC32_assemble_rule target)
    set(options
        "-g"
        "${ASSEMBLER_PRE}"
        "-mprocessor=32CM6408PL10048"
        "-Wa,--defsym=__MPLAB_BUILD=1${MP_EXTRA_AS_POST}"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC32CM-PL_DFP/1.4.418/pic32cm6408pl")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
endfunction()
function(DinoGame_default_default_XC32_assembleWithPreprocess_rule target)
    set(options
        "-x"
        "assembler-with-cpp"
        "-g"
        "${MP_EXTRA_AS_PRE}"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC32CM-PL_DFP/1.4.418/pic32cm6408pl"
        "-mprocessor=32CM6408PL10048"
        "-Wa,--defsym=__MPLAB_BUILD=1${MP_EXTRA_AS_POST}")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target} PRIVATE "XPRJ_default=default")
endfunction()
function(DinoGame_default_default_XC32_compile_rule target)
    set(options
        "-g"
        "${CC_PRE}"
        "-x"
        "c"
        "-c"
        "-mprocessor=32CM6408PL10048"
        "-ffunction-sections"
        "-fdata-sections"
        "-O1"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC32CM-PL_DFP/1.4.418/pic32cm6408pl")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target} PRIVATE "XPRJ_default=default")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/packs/CMSIS"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/packs/CMSIS/CMSIS/Core/Include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/packs/PIC32CM6408PL10048_DFP"
        PRIVATE "${PACK_REPO_PATH}/ARM/CMSIS/6.3.0/CMSIS/Core/Include")
endfunction()
function(DinoGame_default_default_XC32_compile_cpp_rule target)
    set(options
        "-g"
        "${CC_PRE}"
        "-mprocessor=32CM6408PL10048"
        "-frtti"
        "-fexceptions"
        "-fno-check-new"
        "-fenforce-eh-specs"
        "-ffunction-sections"
        "-O1"
        "-fno-common"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC32CM-PL_DFP/1.4.418/pic32cm6408pl")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target} PRIVATE "XPRJ_default=default")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/config/default"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/packs/CMSIS"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/packs/CMSIS/CMSIS/Core/Include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../My_MCC_Config/src/packs/PIC32CM6408PL10048_DFP"
        PRIVATE "${PACK_REPO_PATH}/ARM/CMSIS/6.3.0/CMSIS/Core/Include")
endfunction()
function(DinoGame_default_dependentObject_rule target)
    set(options
        "-mprocessor=32CM6408PL10048"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC32CM-PL_DFP/1.4.418/pic32cm6408pl")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
endfunction()
function(DinoGame_default_link_rule target)
    set(options
        "-g"
        "${MP_EXTRA_LD_PRE}"
        "-mprocessor=32CM6408PL10048"
        "-mno-device-startup-code"
        "-Wl,--defsym=__MPLAB_BUILD=1${MP_EXTRA_LD_POST},--script=${DinoGame_default_LINKER_SCRIPT},--defsym=_min_heap_size=512,--gc-sections,-Map=mem.map,--report-mem,--memorysummary,memoryfile.xml"
        "-mdfp=${PACK_REPO_PATH}/Microchip/PIC32CM-PL_DFP/1.4.418/pic32cm6408pl")
    list(REMOVE_ITEM options "")
    target_link_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target} PRIVATE "XPRJ_default=default")
endfunction()
