CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# install edj
ADD_CUSTOM_TARGET(widget_viewer_evas.edj
		COMMAND edje_cc -id ${CMAKE_CURRENT_SOURCE_DIR}/images ${EDJE_CFLAGS}
		${CMAKE_CURRENT_SOURCE_DIR}/widget_viewer_evas.edc widget_viewer_evas.edj
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/widget_viewer_evas.edc
)
ADD_DEPENDENCIES(${PROJECT_NAME} widget_viewer_evas.edj)
INSTALL(FILES widget_viewer_evas.edj DESTINATION ${EDJDIR})
INSTALL(FILES unknown.png DESTINATION ${IMGDIR})

ADD_SUBDIRECTORY(po)
