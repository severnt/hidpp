cmake_minimum_required(VERSION 3.8)
project(hidpp)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_TOOLS "Build HID++ command line tools" ON)
option(INSTALL_UDEV_RULES "Install udev rules for user access to HID++ devices (requires building tools)" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(DEFAULT_HID_BACKEND "linux")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(DEFAULT_HID_BACKEND "windows")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(DEFAULT_HID_BACKEND "macos")
else()
	message(WARNING "System is not supported")
endif()
set(HID_BACKEND "${DEFAULT_HID_BACKEND}" CACHE STRING "Backend used for accessing HID devices")
set_property(CACHE HID_BACKEND PROPERTY STRINGS linux windows macos)

find_package(Threads REQUIRED)
if("${HID_BACKEND}" STREQUAL "linux")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBUDEV libudev REQUIRED)
elseif("${HID_BACKEND}" STREQUAL "windows")
	# add_definitions(-DUNICODE -D_UNICODE)
	# add_definitions(-D_WIN32_WINNT=0x0600) # Use vista or later
elseif("${HID_BACKEND}" STREQUAL "macos")
	set(CMAKE_MACOSX_RPATH ON) # Suppress warning I don't understand
	include_directories(/usr/local/include) 
	# 	^ Prevent `fatal error: 'tinyxml2.h' file not found`
	# 		Src: https://gitlab.kitware.com/cmake/cmake/-/issues/19120
	link_directories(${TINYXML2_LIBRARY_DIRS}) 
	# 	^ Prevent `ld: library not found for -ltinyxml2`
	#		Src: https://stackoverflow.com/q/60362476/10601702
	# set(CMAKE_OSX_ARCHITECTURES arm64;x86_64)
else()
	message(FATAL_ERROR "HID_BACKEND is invalid.")
endif()

include(GNUInstallDirs)

add_subdirectory(src/libhidpp)
add_subdirectory(doc/libhidpp)
if(BUILD_TOOLS)
	add_subdirectory(src/tools)
	if(INSTALL_UDEV_RULES)
		set(PREFIX ${CMAKE_INSTALL_PREFIX})
		configure_file(udev/rules.d/69-hidpp.rules.in udev/rules.d/69-hidpp.rules @ONLY)
		install(FILES ${CMAKE_BINARY_DIR}/udev/rules.d/69-hidpp.rules DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/udev/rules.d)
	endif()
endif()
