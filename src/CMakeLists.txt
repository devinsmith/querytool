cmake_minimum_required(VERSION 3.13.4)

project(querytool)
set(TARGET_NAME querytool)

add_subdirectory(tds)

find_package(FOX REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
#find_package(FreeTDS REQUIRED)

# We cannot be sure cJSON is available as a CMake module, so fall back to pkg-config
#find_package(cjson REQUIRED)
pkg_check_modules(libcjson libcjson REQUIRED)

# C and C++ sources are freely mixed.
set(SOURCES
  Config.cpp Config.h
  main.cpp
  QueryTabBook.cpp QueryTabBook.h
  QueryTabItem.cpp QueryTabItem.h
  QueryTool.cpp QueryTool.h
  Server.h
  ServerEditDlg.cpp ServerEditDlg.h
  ServerTreeList.cpp ServerTreeList.h
  SqlConnection.cpp SqlConnection.h
  icons/root.xpm icons/server.xpm
)

add_executable(${TARGET_NAME} ${SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE ${FOX_INCLUDE_DIR} ${libcjson_INCLUDE_DIRS} tds/include ${CMAKE_BINARY_DIR}/include ${OPENSSL_INCLUDE_DIR})
set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_FLAGS ${FOX_CXX_FLAGS})
target_link_libraries(${TARGET_NAME} PRIVATE ${OPENSSL_LIBRARIES} ${FOX_LIBRARY} ${libcjson_LIBRARIES} tds ${OPENSSL_LIBRARIES})

if(MSVC)
  #  target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
  target_compile_options(${TARGET_NAME} PRIVATE /W3)
else()
  #target_compile_options(${TARGET_NAME} PRIVATE -Wall -Werror)
  target_compile_options(${TARGET_NAME} PRIVATE -Wall)
endif()

