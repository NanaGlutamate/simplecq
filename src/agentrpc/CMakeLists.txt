# file(TIMESTAMP ${CMAKE_CURRENT_SOURCE_DIR}/../cq_agent.proto ORIGINAL_FILE_TIME "%s")
# file(TIMESTAMP ${CMAKE_CURRENT_SOURCE_DIR}/cq_agent.pb.cc GENERATED_FILE_TIME "%s")

# include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# if(GENERATED_FILE_TIME EQUAL "" OR GENERATED_FILE_TIME STRLESS ORIGINAL_FILE_TIME)
#     message(STATUS "[protoc call] regenerate rpc file")
#     # execute_process(COMMAND protoc -I../ --grpc_cpp_out=. ../cq_agent.proto WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
#     execute_process(COMMAND protoc -I../ --cpp_out=. ../cq_agent.proto WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# endif()