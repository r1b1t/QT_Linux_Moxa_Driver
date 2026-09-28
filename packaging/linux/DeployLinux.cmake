foreach(REQUIRED_VAR APP_EXECUTABLE STAGING_DIR APP_ID PROJECT_SOURCE_DIR)
    if(NOT DEFINED ${REQUIRED_VAR})
        message(FATAL_ERROR "DeployLinux.cmake: -D${REQUIRED_VAR} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${STAGING_DIR}")
file(MAKE_DIRECTORY "${STAGING_DIR}/bin")

file(INSTALL "${APP_EXECUTABLE}"
     DESTINATION "${STAGING_DIR}/bin"
     RENAME "${APP_ID}"
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                 GROUP_READ GROUP_EXECUTE
                 WORLD_READ WORLD_EXECUTE)

file(COPY "${PROJECT_SOURCE_DIR}/resources/driver/mxu11x0"
     DESTINATION "${STAGING_DIR}/driver")

file(COPY "${PROJECT_SOURCE_DIR}/resources/scripts/moxa-helper.sh"
     DESTINATION "${STAGING_DIR}/scripts"
     FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                       GROUP_READ GROUP_EXECUTE
                       WORLD_READ WORLD_EXECUTE)

message(STATUS "Staged ${STAGING_DIR} for packaging (app id: ${APP_ID})")
