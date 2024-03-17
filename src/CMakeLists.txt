file(GLOB_RECURSE YAML_SRC ${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/*.cpp)

add_executable(main main.cpp)
add_executable(test test.cpp)
add_library(mymodel SHARED model.cpp ${YAML_SRC})
# add_library(mymodel SHARED model.cpp)

add_dependencies(test mymodel)

if(UNIX)
    target_link_libraries(mymodel dl)
endif(UNIX)

find_package(unifex CONFIG REQUIRED)
target_link_libraries(main PRIVATE unifex::unifex)