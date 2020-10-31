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

#ifndef SRC_PERFETTO_CMD_PACKET_WRITER_H_
#define SRC_PERFETTO_CMD_PACKET_WRITER_H_

#include <memory>
#include <vector>

#include <stdio.h>

#include "perfetto/ext/tracing/core/trace_packet.h"

namespace perfetto {

class PacketWriter {
 public:
  PacketWriter();
  virtual ~PacketWriter();
  virtual bool WritePackets(const std::vector<TracePacket>& packets) {
    for (const TracePacket& packet : packets) {
      if (!WritePacket(packet)) {
        return false;
      }
    }
    return true;
  }
  virtual bool WritePacket(const TracePacket& packets) = 0;
};

std::unique_ptr<PacketWriter> CreateFilePacketWriter(FILE*);
std::unique_ptr<PacketWriter> CreateZipPacketWriter(
    std::unique_ptr<PacketWriter>);

}  // namespace perfetto

#endif  // SRC_PERFETTO_CMD_PACKET_WRITER_H_
