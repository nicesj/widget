PROJECT(widget-mgr C)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/widget-mgr/include)

INCLUDE(FindPkgConfig)
pkg_check_modules(info_pkgs REQUIRED
	dlog
	db-util
	com-core
	ecore
	glib-2.0
	gio-2.0
	widget_service
	xdamage
	xfixes
	x11
	xext
	bundle
)

FOREACH(flag ${info_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -g -Wall -Werror -fPIE")

ADD_DEFINITIONS("-DPREFIX=\"${PREFIX}\"")
ADD_DEFINITIONS("-DSOCKET_FILE=\"/opt/usr/share/live_magazine/.live.socket\"")

ADD_EXECUTABLE(${PROJECT_NAME}
	src/widget-mgr.c
	src/node.c
)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${info_pkgs_LDFLAGS} "-pie")

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION /opt/usr/devel/usr/bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# End of a file
