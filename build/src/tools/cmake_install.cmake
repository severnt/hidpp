# Install script for directory: /Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/src/tools

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp-check-device")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-check-device" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-check-device")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-check-device")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-check-device")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp-list-devices")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-list-devices" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-list-devices")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-list-devices")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-list-devices")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp-list-features")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-list-features" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-list-features")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-list-features")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-list-features")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp-mouse-resolution")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-mouse-resolution" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-mouse-resolution")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-mouse-resolution")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-mouse-resolution")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp10-dump-page")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-dump-page" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-dump-page")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-dump-page")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-dump-page")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp10-write-page")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-write-page" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-write-page")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-write-page")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-write-page")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp10-raw-command")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-raw-command" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-raw-command")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-raw-command")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-raw-command")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp10-active-profile")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-active-profile" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-active-profile")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-active-profile")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-active-profile")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp20-call-function")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-call-function" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-call-function")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-call-function")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-call-function")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp20-onboard-profiles-get-description")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-onboard-profiles-get-description" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-onboard-profiles-get-description")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-onboard-profiles-get-description")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-onboard-profiles-get-description")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp20-reprog-controls")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-reprog-controls" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-reprog-controls")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-reprog-controls")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-reprog-controls")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp20-led-control")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-led-control" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-led-control")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-led-control")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-led-control")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp20-dump-page")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-dump-page" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-dump-page")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-dump-page")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-dump-page")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp20-write-page")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-write-page" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-write-page")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-write-page")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-write-page")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp20-write-data")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-write-data" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-write-data")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-write-data")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp20-write-data")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp-persistent-profiles")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-persistent-profiles" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-persistent-profiles")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-persistent-profiles")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp-persistent-profiles")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/tools/hidpp10-load-temp-profile")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-load-temp-profile" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-load-temp-profile")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/Noah/Documents/Projekte/Programmieren/Xcode/Xcode Projekte/Mac Mouse Fix/hidpp/build/src/libhidpp"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-load-temp-profile")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/hidpp10-load-temp-profile")
    endif()
  endif()
endif()

