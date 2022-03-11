CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# install edj
ADD_CUSTOM_TARGET(widget_viewer_sdk.edj
		COMMAND edje_cc -id ${CMAKE_CURRENT_SOURCE_DIR}/images ${EDJE_CFLAGS}
		${CMAKE_CURRENT_SOURCE_DIR}/widget_viewer_sdk.edc widget_viewer_sdk.edj
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/widget_viewer_sdk.edc
)
ADD_DEPENDENCIES(${PROJECT_NAME} widget_viewer_sdk.edj)
INSTALL(FILES widget_viewer_sdk.edj DESTINATION ${EDJDIR})
INSTALL(FILES images/unknown.png DESTINATION ${SHARED_RESDIR}/images)
