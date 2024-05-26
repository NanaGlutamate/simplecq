file(GLOB_RECURSE YAML_SRC ${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/**.cpp)
file(GLOB_RECURSE GRPC_GEN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/agentrpc/**.cc)

# message(STATUS ${GRPC_GEN_SRC})

add_subdirectory(agentrpc)

add_executable(test test.cpp dllop.cpp engine/console.cpp)
add_executable(tinycq tinycq.cpp dllop.cpp engine/console.cpp)
add_library(mymodel SHARED model.cpp dllop.cpp)
add_library(agent SHARED agent.cpp ${GRPC_GEN_SRC} mysock.cpp)
add_library(yaml ${YAML_SRC})

add_dependencies(mymodel yaml)
add_dependencies(tinycq yaml)

# target_link_libraries(mymodel yaml ws2_32)
target_link_libraries(mymodel yaml)

find_package(mimalloc CONFIG REQUIRED)
if(TARGET mimalloc-static)
message("use static mimalloc")
target_link_libraries(tinycq PRIVATE yaml mimalloc-static)
target_link_libraries(test PRIVATE yaml mimalloc-static)
else()
message("use dynamic mimalloc")
target_link_libraries(tinycq PRIVATE yaml mimalloc)
target_link_libraries(test PRIVATE yaml mimalloc)
endif()

# find_package(gRPC CONFIG REQUIRED)
# target_link_libraries(agent gRPC::grpc++)

if(UNIX)
    target_link_libraries(mymodel dl)
endif(UNIX)

# if(MSVC)
#   set_target_properties(mymodel PROPERTIES LINK_FLAGS " /PROFILE")
#   set_target_properties(test PROPERTIES LINK_FLAGS " /PROFILE")
# endif()

# find_package(unifex CONFIG REQUIRED)
# target_link_libraries(main PRIVATE unifex::unifex)