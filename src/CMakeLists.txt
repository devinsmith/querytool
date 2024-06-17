cmake_minimum_required(VERSION 3.13.4)

project(querytool)
set(TARGET_NAME querytool)

find_package(FOX REQUIRED)

# C and C++ sources are freely mixed.
set(SOURCES
  main.cpp
  QueryTabItem.cpp
  QueryTabItem.h
  QueryTool.cpp
  QueryTool.h
)

add_executable(${TARGET_NAME} ${SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE ${FOX_INCLUDE_DIR})
set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_FLAGS ${FOX_CXX_FLAGS})
target_link_libraries(${TARGET_NAME} PRIVATE ${OPENSSL_LIBRARIES} ${FOX_LIBRARY})

if(MSVC)
  #  target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
  target_compile_options(${TARGET_NAME} PRIVATE /W3)
else()
  #target_compile_options(${TARGET_NAME} PRIVATE -Wall -Werror)
  target_compile_options(${TARGET_NAME} PRIVATE -Wall)
endif()

