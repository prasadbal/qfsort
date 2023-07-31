

FILE(GLOB INCLUDE_FILES *.h)
FILE(GLOB SRC_FILES *.cpp)


add_executable(qfsort ${SRC_FILES} ${INCLUDE_FILES}) 
target_include_directories(qfsort PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(qfsort PROPERTIES LINKER_LANGUAGE CXX)
link_directories(${CMAKE_LIBRARY_PATH})

