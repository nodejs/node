
add_custom_command(
  OUTPUT ${protobuf_source_dir}/conformance/conformance.pb.cc
  DEPENDS protoc ${protobuf_source_dir}/conformance/conformance.proto
  COMMAND protoc ${protobuf_source_dir}/conformance/conformance.proto
      --proto_path=${protobuf_source_dir}/conformance
      --cpp_out=${protobuf_source_dir}/conformance
)

add_custom_command(
  OUTPUT ${protobuf_source_dir}/src/google/protobuf/test_messages_proto3.pb.cc
         ${protobuf_source_dir}/src/google/protobuf/test_messages_proto2.pb.cc
  DEPENDS protoc ${protobuf_source_dir}/src/google/protobuf/test_messages_proto3.proto
          protoc ${protobuf_source_dir}/src/google/protobuf/test_messages_proto2.proto
  COMMAND protoc ${protobuf_source_dir}/src/google/protobuf/test_messages_proto3.proto
                 ${protobuf_source_dir}/src/google/protobuf/test_messages_proto2.proto
      --proto_path=${protobuf_source_dir}/src
      --cpp_out=${protobuf_source_dir}/src
)

add_executable(conformance_test_runner
  ${protobuf_source_dir}/conformance/conformance.pb.cc
  ${protobuf_source_dir}/conformance/conformance_test.cc
  ${protobuf_source_dir}/conformance/binary_json_conformance_main.cc
  ${protobuf_source_dir}/conformance/binary_json_conformance_suite.cc
  ${protobuf_source_dir}/conformance/binary_json_conformance_suite.h
  ${protobuf_source_dir}/conformance/conformance_test_runner.cc
  ${protobuf_source_dir}/conformance/third_party/jsoncpp/json.h
  ${protobuf_source_dir}/conformance/third_party/jsoncpp/jsoncpp.cpp
  ${protobuf_source_dir}/src/google/protobuf/test_messages_proto3.pb.cc
  ${protobuf_source_dir}/src/google/protobuf/test_messages_proto2.pb.cc
)

add_executable(conformance_cpp
  ${protobuf_source_dir}/conformance/conformance.pb.cc
  ${protobuf_source_dir}/conformance/conformance_cpp.cc
  ${protobuf_source_dir}/src/google/protobuf/test_messages_proto3.pb.cc
  ${protobuf_source_dir}/src/google/protobuf/test_messages_proto2.pb.cc
)

target_include_directories(
  conformance_test_runner
  PUBLIC ${protobuf_source_dir}/conformance)

target_include_directories(
  conformance_cpp
  PUBLIC ${protobuf_source_dir}/conformance)

target_link_libraries(conformance_test_runner libprotobuf)
target_link_libraries(conformance_cpp libprotobuf)
