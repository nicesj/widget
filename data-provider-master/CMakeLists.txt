CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(${NAME} C)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkg REQUIRED
	dlog
	aul
	vconf
	sqlite3
	db-util
	glib-2.0
	gio-2.0
	bundle
	ecore
	eina
	com-core
	pkgmgr
	notification
	notification-service
	badge
	badge-service
	libsmack
	shortcut
	pkgmgr-info
	libsystemd-daemon
	libtbm
	capi-appfw-app-manager
)

IF (LIVEBOX)
pkg_check_modules(pkg_widget REQUIRED
	widget_service
)
ENDIF (LIVEBOX)

IF (X11_SUPPORT)
pkg_check_modules(pkg_extra REQUIRED
	ecore-x
	x11
	xfixes
	xext
	xdamage
	libdri2
	dri2proto
)
ENDIF (X11_SUPPORT)

IF (WAYLAND_SUPPORT)
pkg_check_modules(pkg_extra REQUIRED
	ecore-wayland
)
ENDIF (WAYLAND_SUPPORT)

SET(PACKAGE "${PROJECT_NAME}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Winline -Werror -fno-builtin-malloc -fno-omit-frame-pointer -g -fPIE")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)

#ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DPATH_MAX=256")
ADD_DEFINITIONS("-DPACKAGE=\"${PACKAGE}\"")
ADD_DEFINITIONS("-DLOCALEDIR=\"${LOCALEDIR}\"")

ADD_DEFINITIONS("-D_GNU_SOURCE")
ADD_DEFINITIONS("-D_FILE_OFFSET_BITS=64")

ADD_DEFINITIONS("-DINFO_SOCKET=\"/opt/usr/share/live_magazine/.live.socket\"")
ADD_DEFINITIONS("-DCLIENT_SOCKET=\"/tmp/.data-provider-master-client.socket\"")
ADD_DEFINITIONS("-DSLAVE_SOCKET=\"/tmp/.data-provider-master-slave.socket\"")
ADD_DEFINITIONS("-DSERVICE_SOCKET=\"/tmp/.data-provider-master-service.socket\"")
ADD_DEFINITIONS("-DSHARED_SOCKET=\"/tmp/.data-provider-master-fd.socket\"")

ADD_DEFINITIONS("-DCATEGORY_WATCH_CLOCK=\"http://tizen.org/category/wearable_clock\"")

ADD_DEFINITIONS("-DCLIENT_PORT=\"8208\"")

ADD_DEFINITIONS("-DBADGE_SOCKET=\"/tmp/.badge.service\"")
ADD_DEFINITIONS("-DSHORTCUT_SOCKET=\"/tmp/.shortcut.service\"")
ADD_DEFINITIONS("-DNOTIFICATION_SOCKET=\"/tmp/.notification.service\"")
ADD_DEFINITIONS("-DUTILITY_SOCKET=\"/tmp/.utility.service\"")

ADD_DEFINITIONS("-DUTILITY_SMACK_LABEL=NULL")
ADD_DEFINITIONS("-DSHORTCUT_SMACK_LABEL=NULL")
ADD_DEFINITIONS("-DNOTIFICATION_SMACK_LABEL=NULL")
ADD_DEFINITIONS("-DBADGE_SMACK_LABEL=NULL")
ADD_DEFINITIONS("-DDATA_SHARE_LABEL=\"${PROJECT_NAME}::share\"")
ADD_DEFINITIONS("-DDEFAULT_SMACK_LABEL=\"${PROJECT_NAME}\"")

ADD_DEFINITIONS("-DDEFAULT_MASTER_CONF=\"/usr/share/${PROJECT_NAME}/conf.ini\"")

ADD_DEFINITIONS("-DNDEBUG")

ADD_DEFINITIONS("-D_USE_ECORE_TIME_GET")

IF (MOBILE)
	ADD_DEFINITIONS("-DMOBILE")
ELSEIF (WEARABLE)
	ADD_DEFINITIONS("-DWEARABLE")
ENDIF (MOBILE)

SET(BUILD_SOURCE
	src/main.c
	src/util.c
	src/setting.c
	src/critical_log.c
	src/shortcut_service.c
	src/badge_service.c
	src/notification_service.c
	src/service_common.c
)

IF (LIVEBOX)
	ADD_DEFINITIONS("-DHAVE_LIVEBOX")
	SET(BUILD_SOURCE
		${BUILD_SOURCE}
		src/utility_service.c
		src/dead_monitor.c
		src/package.c
		src/instance.c
		src/server.c
		src/fault_manager.c
		src/parser.c
		src/io.c
		src/slave_life.c
		src/slave_rpc.c
		src/client_life.c
		src/client_rpc.c
		src/group.c
		src/script_handler.c
		src/widget-mgr.c
		src/pkgmgr.c
		src/event.c
		src/file_service.c
		src/conf.c
		src/monitor.c
		src/shared_fd_service.c
	)

	IF (WAYLAND_SUPPORT)
		ADD_DEFINITIONS("-DHAVE_WAYLAND")
		SET(BUILD_SOURCE
			${BUILD_SOURCE}
			src/xmonitor_wayland.c
			src/buffer_handler_wayland.c
			src/util_wayland.c
		)
	ENDIF (WAYLAND_SUPPORT)

	IF (X11_SUPPORT)
		ADD_DEFINITIONS("-DHAVE_X11")
		SET(BUILD_SOURCE
			${BUILD_SOURCE}
			src/xmonitor.c
			src/buffer_handler.c
			src/util_x11.c
		)
	ENDIF (X11_SUPPORT)
ENDIF (LIVEBOX)

IF (WAYLAND_SUPPORT)
	ADD_DEFINITIONS("-DHAVE_WAYLAND")
	SET(BUILD_SOURCE
		${BUILD_SOURCE}
		src/util_wayland.c
	)
ENDIF (WAYLAND_SUPPORT)

IF (X11_SUPPORT)
	ADD_DEFINITIONS("-DHAVE_X11")
	SET(BUILD_SOURCE
		${BUILD_SOURCE}
		src/util_x11.c
	)
ENDIF (X11_SUPPORT)

STRING(REPLACE "-L-l" "-l" pkg_fixed_LDFLAGS ${pkg_LDFLAGS})
STRING(REPLACE "-l" " -l" pkg_LDFLAGS ${pkg_fixed_LDFLAGS})
STRING(REPLACE "-L" " -L" pkg_fixed_LDFLAGS ${pkg_LDFLAGS})
STRING(REGEX REPLACE "^ " "" pkg_LDFLAGS ${pkg_fixed_LDFLAGS})

#ADD_DEFINITIONS("-D_APPLY_SCRIPT_ASYNC_UPDATE")
#ADD_DEFINITIONS("-DFLOG")
ADD_DEFINITIONS(${pkg_CFLAGS})
ADD_DEFINITIONS(${pkg_LDFLAGS})
ADD_DEFINITIONS(${pkg_extra_CFLAGS})
ADD_DEFINITIONS(${pkg_extra_LDFLAGS})
ADD_DEFINITIONS(${pkg_widget_CFLAGS})
ADD_DEFINITIONS(${pkg_widget_LDFLAGS})

ADD_EXECUTABLE(${PROJECT_NAME} ${BUILD_SOURCE})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkg_LDFLAGS} ${pkg_extra_LDFLAGS} ${pkg_widget_LDFLAGS} "-ldl -lrt -pie")

IF (LIVEBOX)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/abi.ini DESTINATION /usr/share/${PROJECT_NAME} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/${PRODUCT}.conf.ini DESTINATION /usr/share/${PROJECT_NAME} RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/${PRODUCT}.resolution.ini DESTINATION /usr/share/${PROJECT_NAME} RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.mobile.320x480.conf.ini DESTINATION /usr/share/${PROJECT_NAME}/320x480 RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.mobile.320x480.resolution.ini DESTINATION /usr/share/${PROJECT_NAME}/320x480 RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.mobile.480x800.conf.ini DESTINATION /usr/share/${PROJECT_NAME}/480x800 RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.mobile.480x800.resolution.ini DESTINATION /usr/share/${PROJECT_NAME}/480x800 RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.wearable.360x480.conf.ini DESTINATION /usr/share/${PROJECT_NAME}/360x480 RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.wearable.360x480.resolution.ini DESTINATION /usr/share/${PROJECT_NAME}/360x480 RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.wearable.360x360.conf.ini DESTINATION /usr/share/${PROJECT_NAME}/360x360 RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.wearable.360x360.resolution.ini DESTINATION /usr/share/${PROJECT_NAME}/360x360 RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.mobile.conf.ini DESTINATION /usr/share/${PROJECT_NAME}/720x1280 RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.mobile.resolution.ini DESTINATION /usr/share/${PROJECT_NAME}/720x1280 RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.wearable.conf.ini DESTINATION /usr/share/${PROJECT_NAME}/320x320 RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.wearable.resolution.ini DESTINATION /usr/share/${PROJECT_NAME}/320x320 RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.mobile.540x960.conf.ini DESTINATION /usr/share/${PROJECT_NAME}/540x960 RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/x11.mobile.540x960.resolution.ini DESTINATION /usr/share/${PROJECT_NAME}/540x960 RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/${PRODUCT}.conf.ini DESTINATION /usr/share/${PROJECT_NAME} RENAME "conf.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/data/${TARGET}/${PRODUCT}.resolution.ini DESTINATION /usr/share/${PROJECT_NAME} RENAME "resolution.ini" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
ENDIF (LIVEBOX)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION /usr/share/license RENAME "${PROJECT_NAME}")
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}.rule DESTINATION /etc/smack/accesses.d RENAME ${PROJECT_NAME})

# INCLUDE FOR BUILD & INSTALL .PO FILES
ADD_SUBDIRECTORY(res)
ADD_SUBDIRECTORY(data)
IF (LIVEBOX)
ADD_SUBDIRECTORY(pkgmgr_widget)
ENDIF (LIVEBOX)

IF (X11_SUPPORT)
	IF ("${ENGINEER_BINARY}" STREQUAL "true")
		ADD_SUBDIRECTORY(widget-mgr)
	ENDIF ("${ENGINEER_BINARY}" STREQUAL "true")
ENDIF (X11_SUPPORT)

# End of a file
