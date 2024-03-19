file(TIMESTAMP ${CMAKE_CURRENT_SOURCE_DIR}/cq_agent.proto ORIGINAL_FILE_TIME "%s")
file(TIMESTAMP ${CMAKE_CURRENT_SOURCE_DIR}/lex.yy.c GENERATED_FILE_TIME "%s")

if(GENERATED_FILE_TIME EQUAL "" OR GENERATED_FILE_TIME STRLESS ORIGINAL_FILE_TIME)
    message(STATUS "[protoc call] regenerate rpc file")
    execute_process(COMMAND python -m grpc_tools.protoc -I./ --cpp_out=. ./cq_agent.proto WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()