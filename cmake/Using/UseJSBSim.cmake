MACRO(USE_JSBSIM)
  covise_find_package(JSBSIM)
  IF ((NOT JSBSIM_FOUND) AND (${ARGC} LESS 1))
    USING_MESSAGE("Skipping because of missing JSBSIM")
    RETURN()
  ENDIF((NOT JSBSIM_FOUND) AND (${ARGC} LESS 1))
  IF(NOT JSBSIM_USED AND JSBSIM_FOUND)
    SET(JSBSIM_USED TRUE)
    ADD_DEFINITIONS(-DJSBSIM_STATIC_LINK)
    INCLUDE_DIRECTORIES(SYSTEM ${JSBSIM_INCLUDE_DIR} ${JSBSIM_INCLUDE_DIR}/JSBSim)
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${JSBSIM_LIBRARIES})
  ENDIF()
ENDMACRO(USE_JSBSIM)
  
