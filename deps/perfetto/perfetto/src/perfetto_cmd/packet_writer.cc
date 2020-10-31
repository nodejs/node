/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/perfetto_cmd/packet_writer.h"

#include <array>

#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/protozero/proto_utils.h"

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
#include <zlib.h>
#endif

namespace perfetto {
namespace {

using protozero::proto_utils::kMessageLengthFieldSize;
using protozero::proto_utils::MakeTagLengthDelimited;
using protozero::proto_utils::WriteRedundantVarInt;
using protozero::proto_utils::WriteVarInt;
using Preamble = std::array<char, 16>;

// ID of the |packet| field in trace.proto. Hardcoded as this we don't
// want to depend on protos/trace:lite for binary size saving reasons.
constexpr uint32_t kPacketId = 1;

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)

// ID of |compressed_packets| in trace_packet.proto.
constexpr uint32_t kCompressedPacketsId = 50;

// Some transport mechanisms have a 512kb limit on packet size.
// ZipPacketWriter respects this limit where possible and does
// not produce compressed packets larger than 512kb. This is
// constant is deliberately conservative to leave plenty of
// room for the transport to add additional headers etc.
const size_t kMaxPacketSize = 500 * 1024;

// After every kPendingBytesLimit we do a Z_SYNC_FLUSH in the zlib stream.
const size_t kPendingBytesLimit = 32 * 1024;

#endif  // PERFETTO_BUILDFLAG(PERFETTO_ZLIB)

template <uint32_t id>
size_t GetPreamble(size_t sz, Preamble* preamble) {
  uint8_t* ptr = reinterpret_cast<uint8_t*>(preamble->data());
  constexpr uint32_t tag = MakeTagLengthDelimited(id);
  ptr = WriteVarInt(tag, ptr);
  ptr = WriteVarInt(sz, ptr);
  size_t preamble_size = reinterpret_cast<uintptr_t>(ptr) -
                         reinterpret_cast<uintptr_t>(preamble->data());
  PERFETTO_DCHECK(preamble_size < preamble->size());
  return preamble_size;
}

class FilePacketWriter : public PacketWriter {
 public:
  FilePacketWriter(FILE* fd);
  ~FilePacketWriter() override;
  bool WritePacket(const TracePacket& packet) override;

 private:
  FILE* fd_;
};

FilePacketWriter::FilePacketWriter(FILE* fd) : fd_(fd) {}

FilePacketWriter::~FilePacketWriter() {
  fflush(fd_);
}

bool FilePacketWriter::WritePacket(const TracePacket& packet) {
  Preamble preamble;
  size_t size = GetPreamble<kPacketId>(packet.size(), &preamble);
  if (fwrite(preamble.data(), 1, size, fd_) != size)
    return false;
  for (const Slice& slice : packet.slices()) {
    if (fwrite(reinterpret_cast<const char*>(slice.start), 1, slice.size,
               fd_) != slice.size) {
      return false;
    }
  }

  return true;
}

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)

class ZipPacketWriter : public PacketWriter {
 public:
  ZipPacketWriter(std::unique_ptr<PacketWriter>);
  ~ZipPacketWriter() override;
  bool WritePacket(const TracePacket& packet) override;

 private:
  void CheckEq(int actual_code, int expected_code);
  bool FinalizeCompressedPacket();
  inline void Deflate(const char* ptr, size_t size) {
    return Deflate(reinterpret_cast<const uint8_t*>(ptr), size);
  }
  inline void Deflate(const void* ptr, size_t size) {
    return Deflate(reinterpret_cast<const uint8_t*>(ptr), size);
  }
  void Deflate(const uint8_t* ptr, size_t size);

  std::unique_ptr<PacketWriter> writer_;
  z_stream stream_{};

  base::PagedMemory buf_;
  uint8_t* const start_;
  uint8_t* const end_;

  bool is_compressing_ = false;
  size_t pending_bytes_ = 0;
};

ZipPacketWriter::ZipPacketWriter(std::unique_ptr<PacketWriter> writer)
    : writer_(std::move(writer)),
      buf_(base::PagedMemory::Allocate(kMaxPacketSize)),
      start_(static_cast<uint8_t*>(buf_.Get())),
      end_(start_ + buf_.size()) {}

ZipPacketWriter::~ZipPacketWriter() {
  if (is_compressing_)
    FinalizeCompressedPacket();
}

bool ZipPacketWriter::WritePacket(const TracePacket& packet) {
  // If we have already written one compressed packet, check whether we should
  // flush the buffer.
  if (is_compressing_) {
    // We have two goals:
    // - Fit as much data as possible into each packet
    // - Ensure each packet is under 512KB
    // We keep track of two numbers:
    // - the number of remaining bytes in the output buffer
    // - the number of (pending) uncompressed bytes written since the last flush
    // The pending bytes may or may not have appeared in output buffer.
    // Assuming in the worst case each uncompressed input byte can turn into
    // two compressed bytes we can ensure we don't go over 512KB by not letting
    // the number of pending bytes go over remaining bytes/2 - however often
    // each input byte will not turn into 2 output bytes but less than 1 output
    // byte - so this underfills the packet. To avoid this every 32kb we deflate
    // with Z_SYNC_FLUSH ensuring all pending bytes are present in the output
    // buffer.
    if (pending_bytes_ > kPendingBytesLimit) {
      CheckEq(deflate(&stream_, Z_SYNC_FLUSH), Z_OK);
      pending_bytes_ = 0;
    }

    PERFETTO_DCHECK(end_ >= stream_.next_out);
    size_t remaining = static_cast<size_t>(end_ - stream_.next_out);
    if ((pending_bytes_ + packet.size() + 1024) * 2 > remaining) {
      if (!FinalizeCompressedPacket()) {
        return false;
      }
    }
  }

  // Do not attempt to compress large packets (since they may overflow our
  // output buffer) instead insert them directly into the output stream:
  if (packet.size() > kMaxPacketSize) {
    // We can't be compressing here, if we were we would have attempted
    // to FinalizeCompressedPacket() above:
    PERFETTO_DCHECK(!is_compressing_);
    return writer_->WritePacket(packet);
  }

  // Reinitialize the compresser if needed:
  if (!is_compressing_) {
    memset(&stream_, 0, sizeof(stream_));
    CheckEq(deflateInit(&stream_, 6), Z_OK);
    is_compressing_ = true;
    stream_.next_out = start_;
    stream_.avail_out = static_cast<unsigned int>(end_ - start_);
  }

  // Compress the trace packet header:
  Preamble packet_hdr;
  size_t packet_hdr_size = GetPreamble<kPacketId>(packet.size(), &packet_hdr);
  Deflate(packet_hdr.data(), packet_hdr_size);

  // Compress the trace packet itself:
  for (const Slice& slice : packet.slices()) {
    Deflate(slice.start, slice.size);
  }

  return true;
}

bool ZipPacketWriter::FinalizeCompressedPacket() {
  PERFETTO_DCHECK(is_compressing_);

  CheckEq(deflate(&stream_, Z_FINISH), Z_STREAM_END);

  size_t size = static_cast<size_t>(stream_.next_out - start_);
  Preamble preamble;
  size_t preamble_size = GetPreamble<kCompressedPacketsId>(size, &preamble);

  std::vector<TracePacket> out_packets(1);
  TracePacket& out_packet = out_packets[0];
  out_packet.AddSlice(preamble.data(), preamble_size);
  out_packet.AddSlice(start_, size);

  if (!writer_->WritePackets(out_packets))
    return false;

  is_compressing_ = false;
  pending_bytes_ = 0;
  CheckEq(deflateEnd(&stream_), Z_OK);
  return true;
}

void ZipPacketWriter::CheckEq(int actual_code, int expected_code) {
  if (actual_code == expected_code)
    return;
  PERFETTO_FATAL("Expected %d got %d: %s", actual_code, expected_code,
                 stream_.msg);
}

void ZipPacketWriter::Deflate(const uint8_t* ptr, size_t size) {
  PERFETTO_CHECK(is_compressing_);
  stream_.next_in = const_cast<uint8_t*>(ptr);
  stream_.avail_in = static_cast<unsigned int>(size);
  CheckEq(deflate(&stream_, Z_NO_FLUSH), Z_OK);
  PERFETTO_CHECK(stream_.avail_in == 0);
  pending_bytes_ += size;
}

#endif  // PERFETTO_BUILDFLAG(PERFETTO_ZLIB)

}  // namespace

PacketWriter::PacketWriter() {}

PacketWriter::~PacketWriter() {}

std::unique_ptr<PacketWriter> CreateFilePacketWriter(FILE* fd) {
  return std::unique_ptr<PacketWriter>(new FilePacketWriter(fd));
}

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
std::unique_ptr<PacketWriter> CreateZipPacketWriter(
    std::unique_ptr<PacketWriter> writer) {
  return std::unique_ptr<PacketWriter>(new ZipPacketWriter(std::move(writer)));
}
#endif

}  // namespace perfetto
