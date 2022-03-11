CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(widget_edje C)

INCLUDE(FindPkgConfig)
pkg_check_modules(live_edje REQUIRED
	eina
	evas
	edje
	dlog
	eet
	ecore
	vconf
	widget_service
	elementary
	capi-system-system-settings
)

FOREACH (flag ${live_edje_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DPATH_MAX=256")
ADD_DEFINITIONS("-DPACKAGE=\"${PROJECT_NAME}\"")

ADD_DEFINITIONS("${EXTRA_CFLAGS} -g -Wall -Werror")
ADD_DEFINITIONS("-DNDEBUG")
ADD_DEFINITIONS("-D_USE_ECORE_TIME_GET")
ADD_DEFINITIONS("-DLOG_TAG=\"WIDGET_EDJE\"")

ADD_LIBRARY(${PROJECT_NAME} SHARED
	src/script_port.c
	src/abi.c
)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${live_edje_LDFLAGS} "-ldl")

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION "/usr/share/data-provider-master/plugin-script")
INSTALL(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION /usr/share/license RENAME "lib${PROJECT_NAME}")
