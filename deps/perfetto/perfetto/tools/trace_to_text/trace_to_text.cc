/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tools/trace_to_text/trace_to_text.h"

#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/scoped_file.h"
#include "tools/trace_to_text/proto_full_utils.h"
#include "tools/trace_to_text/utils.h"

#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
#include <zlib.h>
#endif

namespace perfetto {
namespace trace_to_text {

namespace {
using google::protobuf::Descriptor;
using google::protobuf::DynamicMessageFactory;
using google::protobuf::FieldDescriptor;
using google::protobuf::FileDescriptor;
using google::protobuf::Message;
using google::protobuf::Reflection;
using google::protobuf::TextFormat;
using google::protobuf::compiler::DiskSourceTree;
using google::protobuf::compiler::Importer;
using google::protobuf::io::OstreamOutputStream;
using google::protobuf::io::ZeroCopyOutputStream;

inline void WriteToZeroCopyOutput(ZeroCopyOutputStream* output,
                                  const char* str,
                                  size_t length) {
  if (length == 0)
    return;

  void* data;
  int size = 0;
  size_t bytes_to_copy = 0;
  while (length) {
    output->Next(&data, &size);
    bytes_to_copy = std::min(length, static_cast<size_t>(size));
    memcpy(data, str, bytes_to_copy);
    length -= bytes_to_copy;
    str += bytes_to_copy;
  }
  output->BackUp(size - static_cast<int>(bytes_to_copy));
}

constexpr char kCompressedPacketsPrefix[] = "compressed_packets {\n";
constexpr char kCompressedPacketsSuffix[] = "}\n";

constexpr char kIndentedPacketPrefix[] = "  packet {\n";
constexpr char kIndentedPacketSuffix[] = "  }\n";

constexpr char kPacketPrefix[] = "packet {\n";
constexpr char kPacketSuffix[] = "}\n";

void PrintCompressedPackets(const std::string& packets,
                            Message* compressed_msg_scratch,
                            ZeroCopyOutputStream* output) {
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
  uint8_t out[4096];
  std::vector<uint8_t> data;

  z_stream stream{};
  stream.next_in =
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packets.data()));
  stream.avail_in = static_cast<unsigned int>(packets.length());

  if (inflateInit(&stream) != Z_OK) {
    PERFETTO_ELOG("Error when initiliazing zlib to decompress packets");
    return;
  }

  int ret;
  do {
    stream.next_out = out;
    stream.avail_out = sizeof(out);
    ret = inflate(&stream, Z_NO_FLUSH);
    if (ret != Z_STREAM_END && ret != Z_OK) {
      PERFETTO_ELOG("Error when decompressing packets");
      return;
    }
    data.insert(data.end(), out, out + (sizeof(out) - stream.avail_out));
  } while (ret != Z_STREAM_END);
  inflateEnd(&stream);

  protos::pbzero::Trace::Decoder decoder(data.data(), data.size());
  WriteToZeroCopyOutput(output, kCompressedPacketsPrefix,
                        sizeof(kCompressedPacketsPrefix) - 1);
  TextFormat::Printer printer;
  printer.SetInitialIndentLevel(2);
  for (auto it = decoder.packet(); it; ++it) {
    protozero::ConstBytes cb = *it;
    compressed_msg_scratch->ParseFromArray(cb.data, static_cast<int>(cb.size));
    WriteToZeroCopyOutput(output, kIndentedPacketPrefix,
                          sizeof(kIndentedPacketPrefix) - 1);
    printer.Print(*compressed_msg_scratch, output);
    WriteToZeroCopyOutput(output, kIndentedPacketSuffix,
                          sizeof(kIndentedPacketSuffix) - 1);
  }
  WriteToZeroCopyOutput(output, kCompressedPacketsSuffix,
                        sizeof(kCompressedPacketsSuffix) - 1);
#else
  base::ignore_result(packets);
  base::ignore_result(compressed_msg_scratch);
  base::ignore_result(kIndentedPacketPrefix);
  base::ignore_result(kIndentedPacketSuffix);
  WriteToZeroCopyOutput(output, kCompressedPacketsPrefix,
                        sizeof(kCompressedPacketsPrefix) - 1);
  static const char kErrMsg[] =
      "Cannot decode compressed packets. zlib not enabled in the build config";
  WriteToZeroCopyOutput(output, kErrMsg, sizeof(kErrMsg) - 1);
  WriteToZeroCopyOutput(output, kCompressedPacketsSuffix,
                        sizeof(kCompressedPacketsSuffix) - 1);
  static bool log_once = [] {
    PERFETTO_ELOG("%s", kErrMsg);
    return true;
  }();
  base::ignore_result(log_once);
#endif  // PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
}

}  // namespace

int TraceToText(std::istream* input, std::ostream* output) {
  const std::string proto_path = "protos/perfetto/trace/trace_packet.proto";

  if (!base::OpenFile(proto_path, O_RDONLY)) {
    PERFETTO_ELOG("Cannot open %s.", proto_path.c_str());
    PERFETTO_ELOG(
        "Text mode only works from the perfetto directory. Googlers, see "
        "b/131425913");
    return 1;
  }

  DiskSourceTree dst;
  dst.MapPath("", "");
  MultiFileErrorCollectorImpl mfe;
  Importer importer(&dst, &mfe);
  const FileDescriptor* parsed_file =
      importer.Import("protos/perfetto/trace/trace_packet.proto");

  DynamicMessageFactory dmf;
  const Descriptor* trace_descriptor = parsed_file->message_type(0);
  const Message* root = dmf.GetPrototype(trace_descriptor);
  OstreamOutputStream zero_copy_output(output);
  OstreamOutputStream* zero_copy_output_ptr = &zero_copy_output;
  Message* msg = root->New();

  constexpr uint32_t kCompressedPacketFieldDescriptor = 50;
  const Reflection* reflect = msg->GetReflection();
  const FieldDescriptor* compressed_desc =
      trace_descriptor->FindFieldByNumber(kCompressedPacketFieldDescriptor);
  Message* compressed_msg_scratch = root->New();
  std::string compressed_packet_scratch;

  TextFormat::Printer printer;
  printer.SetInitialIndentLevel(1);
  ForEachPacketBlobInTrace(
      input, [msg, reflect, compressed_desc, zero_copy_output_ptr,
              &compressed_packet_scratch, compressed_msg_scratch,
              &printer](std::unique_ptr<char[]> buf, size_t size) {
        if (!msg->ParseFromArray(buf.get(), static_cast<int>(size))) {
          PERFETTO_ELOG("Skipping invalid packet");
          return;
        }
        if (reflect->HasField(*msg, compressed_desc)) {
          const auto& compressed_packets = reflect->GetStringReference(
              *msg, compressed_desc, &compressed_packet_scratch);
          PrintCompressedPackets(compressed_packets, compressed_msg_scratch,
                                 zero_copy_output_ptr);
        } else {
          WriteToZeroCopyOutput(zero_copy_output_ptr, kPacketPrefix,
                                sizeof(kPacketPrefix) - 1);
          printer.Print(*msg, zero_copy_output_ptr);
          WriteToZeroCopyOutput(zero_copy_output_ptr, kPacketSuffix,
                                sizeof(kPacketSuffix) - 1);
        }
      });
  return 0;
}

}  // namespace trace_to_text
}  // namespace perfetto
