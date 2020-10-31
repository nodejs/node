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

#ifndef SRC_TRACED_PROBES_FTRACE_EVENT_INFO_CONSTANTS_H_
#define SRC_TRACED_PROBES_FTRACE_EVENT_INFO_CONSTANTS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/protozero/proto_utils.h"

namespace perfetto {

enum FtraceFieldType {
  kInvalidFtraceFieldType = 0,
  kFtraceUint8,
  kFtraceUint16,
  kFtraceUint32,
  kFtraceUint64,
  kFtraceInt8,
  kFtraceInt16,
  kFtraceInt32,
  kFtraceInt64,
  kFtraceFixedCString,
  kFtraceCString,
  kFtraceStringPtr,
  kFtraceBool,
  kFtraceInode32,
  kFtraceInode64,
  kFtracePid32,
  kFtraceCommonPid32,
  kFtraceDevId32,
  kFtraceDevId64,
  kFtraceDataLoc,
};

// Joint enum of FtraceFieldType (left) and ProtoFieldType (right).
// where there exists a way to convert from the FtraceFieldType
// into the ProtoFieldType.
enum TranslationStrategy {
  kInvalidTranslationStrategy = 0,
  kUint8ToUint32,
  kUint8ToUint64,
  kUint16ToUint32,
  kUint16ToUint64,
  kUint32ToUint32,
  kUint32ToUint64,
  kUint64ToUint64,
  kInt8ToInt32,
  kInt8ToInt64,
  kInt16ToInt32,
  kInt16ToInt64,
  kInt32ToInt32,
  kInt32ToInt64,
  kInt64ToInt64,
  kFixedCStringToString,
  kCStringToString,
  kStringPtrToString,
  kBoolToUint32,
  kBoolToUint64,
  kInode32ToUint64,
  kInode64ToUint64,
  kPid32ToInt32,
  kPid32ToInt64,
  kCommonPid32ToInt32,
  kCommonPid32ToInt64,
  kDevId32ToUint64,
  kDevId64ToUint64,
  kDataLocToString,
};

inline const char* ToString(FtraceFieldType v) {
  switch (v) {
    case kFtraceUint8:
      return "uint8";
    case kFtraceUint16:
      return "uint16";
    case kFtraceUint32:
      return "uint32";
    case kFtraceUint64:
      return "uint64";
    case kFtraceInt8:
      return "int8";
    case kFtraceInt16:
      return "int16";
    case kFtraceInt32:
      return "int32";
    case kFtraceInt64:
      return "int64";
    case kFtraceFixedCString:
      return "fixed length null terminated string";
    case kFtraceCString:
      return "null terminated string";
    case kFtraceStringPtr:
      return "string ptr";
    case kFtraceBool:
      return "bool";
    case kFtraceInode32:
      return "inode32";
    case kFtraceInode64:
      return "inode64";
    case kFtracePid32:
      return "pid32";
    case kFtraceCommonPid32:
      return "common_pid32";
    case kFtraceDevId32:
      return "devid32";
    case kFtraceDevId64:
      return "devid64";
    case kFtraceDataLoc:
      return "__data_loc";
    case kInvalidFtraceFieldType:
      break;
  }
  PERFETTO_FATAL("Unexpected ftrace field type.");
}

struct Field {
  uint16_t ftrace_offset;
  uint16_t ftrace_size;
  FtraceFieldType ftrace_type;
  const char* ftrace_name;

  uint32_t proto_field_id;
  protozero::proto_utils::ProtoSchemaType proto_field_type;

  TranslationStrategy strategy;
};

struct Event {
  const char* name;
  const char* group;
  std::vector<Field> fields;
  uint32_t ftrace_event_id;

  // Field id of the subevent proto (e.g. PrintFtraceEvent) in the FtraceEvent
  // parent proto.
  uint32_t proto_field_id;

  // 'Size' of the event. Some caveats: some events (e.g. print) end with a null
  // terminated string of unknown size. This size doesn't include the length of
  // that string.
  uint16_t size;
};

// The compile time information needed to read the common fields from
// the raw ftrace buffer.
std::vector<Field> GetStaticCommonFieldsInfo();

bool SetTranslationStrategy(FtraceFieldType ftrace,
                            protozero::proto_utils::ProtoSchemaType proto,
                            TranslationStrategy* out);

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_EVENT_INFO_CONSTANTS_H_
