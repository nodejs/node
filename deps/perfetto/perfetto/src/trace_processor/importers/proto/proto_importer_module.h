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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_IMPORTER_MODULE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_IMPORTER_MODULE_H_

#include "perfetto/ext/base/optional.h"
#include "perfetto/trace_processor/status.h"
#include "src/trace_processor/trace_blob_view.h"

namespace perfetto {

namespace protos {
namespace pbzero {
class TraceConfig_Decoder;
class TracePacket_Decoder;
}  // namespace pbzero
}  // namespace protos

namespace trace_processor {

class PacketSequenceState;
struct TimestampedTracePiece;
class TraceProcessorContext;

// This file contains a base class for ProtoTraceTokenizer/Parser modules.
// A module implements support for a subset of features of the TracePacket
// proto format.
// To add and integrate a new module:
// (1) Add MyModule as a subclass of ProtoImporterModule,
//     overriding the TokenizePacket(), ParsePacket() and/or ParseTraceConfig()
//     methods.
// (2) In the constructor call the RegisterForField method for every field
//     that the module knows how to handle.
// (3) Create a module instance and add it to TraceProcessorContext's |modules|
//     vector in either default_modules.cc or additional_modules.cc.
// See GraphicsEventModule for an example.

class ModuleResult {
 public:
  // Allow auto conversion from util::Status to Handled / Error result.
  ModuleResult(util::Status status)
      : ignored_(false),
        error_(status.ok() ? base::nullopt
                           : base::make_optional(status.message())) {}

  // Constructs a result that indicates the module ignored the packet and is
  // deferring the handling of the packet to other modules.
  static ModuleResult Ignored() { return ModuleResult(true); }

  // Constructs a result that indicates the module handled the packet. Other
  // modules will not be notified about the packet.
  static ModuleResult Handled() { return ModuleResult(false); }

  // Constructs a result that indicates an error condition while handling the
  // packet. Other modules will not be notified about the packet.
  static ModuleResult Error(const std::string& message) {
    return ModuleResult(message);
  }

  bool ignored() const { return ignored_; }
  bool ok() const { return !error_.has_value(); }
  const std::string& message() const { return *error_; }

  util::Status ToStatus() const {
    PERFETTO_DCHECK(!ignored_);
    if (error_)
      return util::Status(*error_);
    return util::OkStatus();
  }

 private:
  explicit ModuleResult(bool ignored) : ignored_(ignored) {}
  explicit ModuleResult(const std::string& error)
      : ignored_(false), error_(error) {}

  bool ignored_;
  base::Optional<std::string> error_;
};

// Base class for modules.
class ProtoImporterModule {
 public:
  ProtoImporterModule();

  virtual ~ProtoImporterModule();

  // Called by ProtoTraceTokenizer during the tokenization stage, i.e. before
  // sorting. It's called for each TracePacket that contains fields for which
  // the module was registered. If this returns a result other than
  // ModuleResult::Ignored(), tokenization of the packet will be aborted after
  // the module.
  virtual ModuleResult TokenizePacket(
      const protos::pbzero::TracePacket_Decoder&,
      TraceBlobView* packet,
      int64_t packet_timestamp,
      PacketSequenceState*,
      uint32_t field_id);

  // Called by ProtoTraceParser after the sorting stage for each non-ftrace
  // TracePacket that contains fields for which the module was registered.
  virtual void ParsePacket(const protos::pbzero::TracePacket_Decoder&,
                           const TimestampedTracePiece&,
                           uint32_t field_id);

  // Called by ProtoTraceParser for trace config packets after the sorting
  // stage, on all existing modules.
  virtual void ParseTraceConfig(const protos::pbzero::TraceConfig_Decoder&);

  virtual void NotifyEndOfFile() {}

 protected:
  void RegisterForField(uint32_t field_id, TraceProcessorContext*);
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROTO_IMPORTER_MODULE_H_
