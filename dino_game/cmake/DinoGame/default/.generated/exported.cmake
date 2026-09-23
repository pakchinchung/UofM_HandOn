set(DEPENDENT_MP_BIN2HEXDinoGame_default_3WpNnpC2 "c:/Program Files/Microchip/xc32/v5.10/bin/xc32-bin2hex.exe")
set(DEPENDENT_DEPENDENT_TARGET_ELFDinoGame_default_3WpNnpC2 ${CMAKE_CURRENT_LIST_DIR}/../../../../out/DinoGame/default.elf)
set(DEPENDENT_TARGET_DIRDinoGame_default_3WpNnpC2 ${CMAKE_CURRENT_LIST_DIR}/../../../../out/DinoGame)
set(DEPENDENT_BYPRODUCTSDinoGame_default_3WpNnpC2 ${DEPENDENT_TARGET_DIRDinoGame_default_3WpNnpC2}/${sourceFileNameDinoGame_default_3WpNnpC2}.c)
add_custom_command(
    OUTPUT ${DEPENDENT_TARGET_DIRDinoGame_default_3WpNnpC2}/${sourceFileNameDinoGame_default_3WpNnpC2}.c
    COMMAND ${DEPENDENT_MP_BIN2HEXDinoGame_default_3WpNnpC2} --image ${DEPENDENT_DEPENDENT_TARGET_ELFDinoGame_default_3WpNnpC2} --image-generated-c ${sourceFileNameDinoGame_default_3WpNnpC2}.c --image-generated-h ${sourceFileNameDinoGame_default_3WpNnpC2}.h --image-copy-mode ${modeDinoGame_default_3WpNnpC2} --image-offset ${addressDinoGame_default_3WpNnpC2} 
    WORKING_DIRECTORY ${DEPENDENT_TARGET_DIRDinoGame_default_3WpNnpC2}
    DEPENDS ${DEPENDENT_DEPENDENT_TARGET_ELFDinoGame_default_3WpNnpC2})
add_custom_target(
    dependent_produced_source_artifactDinoGame_default_3WpNnpC2 
    DEPENDS ${DEPENDENT_TARGET_DIRDinoGame_default_3WpNnpC2}/${sourceFileNameDinoGame_default_3WpNnpC2}.c
    )
