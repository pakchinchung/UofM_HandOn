set(DEPENDENT_MP_BIN2HEXTestPl10Again_default_dH5c545c "c:/Program Files/Microchip/xc32/v5.10/bin/xc32-bin2hex.exe")
set(DEPENDENT_DEPENDENT_TARGET_ELFTestPl10Again_default_dH5c545c ${CMAKE_CURRENT_LIST_DIR}/../../../../out/TestPl10Again/default.elf)
set(DEPENDENT_TARGET_DIRTestPl10Again_default_dH5c545c ${CMAKE_CURRENT_LIST_DIR}/../../../../out/TestPl10Again)
set(DEPENDENT_BYPRODUCTSTestPl10Again_default_dH5c545c ${DEPENDENT_TARGET_DIRTestPl10Again_default_dH5c545c}/${sourceFileNameTestPl10Again_default_dH5c545c}.c)
add_custom_command(
    OUTPUT ${DEPENDENT_TARGET_DIRTestPl10Again_default_dH5c545c}/${sourceFileNameTestPl10Again_default_dH5c545c}.c
    COMMAND ${DEPENDENT_MP_BIN2HEXTestPl10Again_default_dH5c545c} --image ${DEPENDENT_DEPENDENT_TARGET_ELFTestPl10Again_default_dH5c545c} --image-generated-c ${sourceFileNameTestPl10Again_default_dH5c545c}.c --image-generated-h ${sourceFileNameTestPl10Again_default_dH5c545c}.h --image-copy-mode ${modeTestPl10Again_default_dH5c545c} --image-offset ${addressTestPl10Again_default_dH5c545c} 
    WORKING_DIRECTORY ${DEPENDENT_TARGET_DIRTestPl10Again_default_dH5c545c}
    DEPENDS ${DEPENDENT_DEPENDENT_TARGET_ELFTestPl10Again_default_dH5c545c})
add_custom_target(
    dependent_produced_source_artifactTestPl10Again_default_dH5c545c 
    DEPENDS ${DEPENDENT_TARGET_DIRTestPl10Again_default_dH5c545c}/${sourceFileNameTestPl10Again_default_dH5c545c}.c
    )
