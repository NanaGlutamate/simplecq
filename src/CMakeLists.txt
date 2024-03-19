file(GLOB_RECURSE YAML_SRC ${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/*.cpp)

add_subdirectory(agentrpc)

add_executable(main main.cpp)
add_executable(test test.cpp)
add_executable(tinycq tinycq.cpp)
add_library(mymodel SHARED model.cpp)
add_library(agent SHARED agent.cpp)
add_library(yaml ${YAML_SRC})
# add_library(mymodel SHARED model.cpp)

add_dependencies(test mymodel)

add_dependencies(mymodel yaml)
target_link_libraries(mymodel yaml)

find_package(gRPC CONFIG REQUIRED)
target_link_libraries(agent gRPC::grpc++)

if(UNIX)
    target_link_libraries(mymodel dl)
endif(UNIX)

# if(MSVC)
#   set_target_properties(mymodel PROPERTIES LINK_FLAGS " /PROFILE")
#   set_target_properties(test PROPERTIES LINK_FLAGS " /PROFILE")
# endif()

find_package(unifex CONFIG REQUIRED)
target_link_libraries(main PRIVATE unifex::unifex)