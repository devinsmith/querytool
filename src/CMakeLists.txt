cmake_minimum_required(VERSION 3.13.4)

project(querytool)
set(TARGET_NAME querytool)

find_package(FOX REQUIRED)
find_package(PkgConfig REQUIRED)

# We cannot be sure cJSON is available as a CMake module, so fall back to pkg-config
#find_package(cjson REQUIRED)
pkg_check_modules(libcjson libcjson REQUIRED)

# C and C++ sources are freely mixed.
set(SOURCES
  Config.cpp Config.h
  main.cpp
  QueryTabItem.cpp QueryTabItem.h
  QueryTool.cpp QueryTool.h
  Server.h
  ServerEditDlg.cpp ServerEditDlg.h
  ServerTreeList.cpp ServerTreeList.h
  icons/root.xpm icons/server.xpm
)

add_executable(${TARGET_NAME} ${SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE ${FOX_INCLUDE_DIR} ${libcjson_INCLUDE_DIRS})
set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_FLAGS ${FOX_CXX_FLAGS})
target_link_libraries(${TARGET_NAME} PRIVATE ${OPENSSL_LIBRARIES} ${FOX_LIBRARY} ${libcjson_LIBRARIES})

if(MSVC)
  #  target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
  target_compile_options(${TARGET_NAME} PRIVATE /W3)
else()
  #target_compile_options(${TARGET_NAME} PRIVATE -Wall -Werror)
  target_compile_options(${TARGET_NAME} PRIVATE -Wall)
endif()

