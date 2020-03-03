cmake_minimum_required(VERSION 3.2)
project(obs-ostws)

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

if (WIN32 OR APPLE)
include(external/FindLibObs.cmake)
endif()

find_package(LibObs REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5WebSockets REQUIRED)
find_package(Qt5Widgets REQUIRED)

set(ENABLE_PROGRAMS false)

set(obs-ostws_SOURCES
	src/obs-ostws.cpp
	src/VideoFilter.cpp
	src/AudioFilter.cpp
	src/WSServer.cpp
	src/WSRequestHandler.cpp
	src/WSRequestHandler_General.cpp
	src/WSRequestHandler_Sources.cpp
	src/WSEvents.cpp
	src/Config.cpp
	src/Utils.cpp)

set(obs-ostws_HEADERS
	src/AudioFilter.h
	src/obs-ostws.h
	src/WSServer.h
	src/WSRequestHandler.h
	src/WSEvents.h
	src/Config.h
	src/Utils.h)

# --- Platform-independent build settings ---
add_library(obs-ostws MODULE 
	${obs-ostws_SOURCES}
	${obs-ostws_HEADERS})

#add_dependencies(obs-ostws)

include_directories( 
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
	${Qt5Core_INCLUDES}
	${Qt5WebSockets_INCLUDES}
	${Qt5Widgets_INCLUDES})

target_link_libraries(obs-ostws 
	libobs
	Qt5::Core
	Qt5::WebSockets
	Qt5::Widgets)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(WIN32)
	if(NOT DEFINED OBS_FRONTEND_LIB)
		set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
		message(FATAL_ERROR "Could not find OBS Frontend API\'s library !")
	endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH_NAME "64bit")
		set(OBS_BUILDDIR_ARCH "build")
	else()
		set(ARCH_NAME "32bit")
		set(OBS_BUILDDIR_ARCH "build")
	endif()

	include_directories(
		"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/UI"
	)

	find_package(w32-pthreads REQUIRED)

	target_link_libraries(obs-ostws
		"${OBS_FRONTEND_LIB}"
		w32-pthreads)

	add_custom_command(TARGET obs-ostws POST_BUILD
		COMMAND if $<CONFIG:Release>==1 ("${CMAKE_COMMAND}" -E copy
			"${QTDIR}/bin/Qt5WebSockets.dll"
			"${QTDIR}/bin/Qt5Network.dll"
			"${CMAKE_BINARY_DIR}/$<CONFIG>")

		COMMAND if $<CONFIG:RelWithDebInfo>==1 ("${CMAKE_COMMAND}" -E copy
			"${QTDIR}/bin/Qt5WebSockets.dll"
			"${QTDIR}/bin/Qt5Network.dll"
			"${CMAKE_BINARY_DIR}/$<CONFIG>")

		COMMAND if $<CONFIG:Debug>==1 ("${CMAKE_COMMAND}" -E copy
			"${QTDIR}/bin/Qt5WebSocketsd.dll"
			"${QTDIR}/bin/Qt5Networkd.dll"
			"${CMAKE_BINARY_DIR}/$<CONFIG>")
	)

	# --- Release package helper ---
	# The "release" folder has a structure similar OBS' one on Windows
	set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/.././obs-studio/build/rundir/Release")

	add_custom_command(TARGET obs-ostws POST_BUILD
		# If config is Release, package release files
		COMMAND if $<CONFIG:Release>==1 (
			"${CMAKE_COMMAND}" -E make_directory
			"${RELEASE_DIR}/data/obs-plugins/obs-ostws"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:Release>==1 ("${CMAKE_COMMAND}" -E copy_directory
			"${PROJECT_SOURCE_DIR}/data"
			"${RELEASE_DIR}/data/obs-plugins/obs-ostws")

		COMMAND if $<CONFIG:Release>==1 ("${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:obs-ostws>"
			"${QTDIR}/bin/Qt5WebSockets.dll"
			"${QTDIR}/bin/Qt5Network.dll"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		# If config is RelWithDebInfo, package release files
		COMMAND if $<CONFIG:RelWithDebInfo>==1 (
			"${CMAKE_COMMAND}" -E make_directory
			"${RELEASE_DIR}/data/obs-plugins/obs-ostws"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:RelWithDebInfo>==1 ("${CMAKE_COMMAND}" -E copy_directory
			"${PROJECT_SOURCE_DIR}/data"
			"${RELEASE_DIR}/data/obs-plugins/obs-ostws")

		COMMAND if $<CONFIG:RelWithDebInfo>==1 ("${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:obs-ostws>"
			"${QTDIR}/bin/Qt5WebSockets.dll"
			"${QTDIR}/bin/Qt5Network.dll"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:RelWithDebInfo>==1 ("${CMAKE_COMMAND}" -E copy
			"$<TARGET_PDB_FILE:obs-ostws>"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		# Copy to obs-studio dev environment for immediate testing
		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy
				"$<TARGET_FILE:obs-ostws>"
				"${QTDIR}/bin/Qt5WebSocketsd.dll"
				"${QTDIR}/bin/Qt5Networkd.dll"
				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy
				"$<TARGET_PDB_FILE:obs-ostws>"
				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E make_directory
				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/obs-ostws")

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy_directory
				"${PROJECT_SOURCE_DIR}/data"
				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/obs-ostws")
	)
	# --- End of sub-section ---

endif()
# --- End of section ---

# --- Linux-specific build settings and tasks ---
if(UNIX AND NOT APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

	
	set_target_properties(obs-ostws PROPERTIES PREFIX "")
	target_link_libraries(obs-ostws
		obs-frontend-api)

	file(GLOB locale_files data/locale/*.ini)

	install(TARGETS obs-ostws
		LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/obs-plugins")
	install(FILES ${locale_files}
		DESTINATION "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/obs-ostws/locale")
endif()
# --- End of section ---

# -- OS X specific build settings and tasks --
if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set(CMAKE_SKIP_RPATH TRUE)
	set_target_properties(obs-ostws PROPERTIES PREFIX "")
	target_link_libraries(obs-ostws "${OBS_FRONTEND_LIB}")
endif()
# -- End of section --
