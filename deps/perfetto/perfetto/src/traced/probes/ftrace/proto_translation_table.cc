/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/traced/probes/ftrace/proto_translation_table.h"

#include <regex.h>
#include <sys/utsname.h>

#include <algorithm>

#include "perfetto/ext/base/string_utils.h"
#include "perfetto/protozero/proto_utils.h"
#include "src/traced/probes/ftrace/event_info.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"

#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "protos/perfetto/trace/ftrace/generic.pbzero.h"

namespace perfetto {

namespace {

using protos::pbzero::GenericFtraceEvent;
using protozero::proto_utils::ProtoSchemaType;

ProtoTranslationTable::FtracePageHeaderSpec MakeFtracePageHeaderSpec(
    const std::vector<FtraceEvent::Field>& fields) {
  ProtoTranslationTable::FtracePageHeaderSpec spec;
  for (const FtraceEvent::Field& field : fields) {
    std::string name = GetNameFromTypeAndName(field.type_and_name);
    if (name == "timestamp")
      spec.timestamp = field;
    else if (name == "commit")
      spec.size = field;
    else if (name == "overwrite")
      spec.overwrite = field;
    else if (name != "data")
      PERFETTO_DFATAL("Invalid field in header spec: %s", name.c_str());
  }
  return spec;
}

// Fallback used when the "header_page" is not readable.
// It uses a hard-coded header_page. The only caveat is that the size of the
// |commit| field depends on the kernel bit-ness. This function tries to infer
// that from the uname() and if that fails assumes that the kernel bitness
// matches the userspace bitness.
ProtoTranslationTable::FtracePageHeaderSpec GuessFtracePageHeaderSpec() {
  ProtoTranslationTable::FtracePageHeaderSpec spec{};
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) && __i386__
  // local_t is arch-specific and models the largest size of an integer that is
  // still atomic across bus transactions, exceptions and IRQ. On android x86
  // this is always size 8
  uint16_t commit_size = 8;
#else
  uint16_t commit_size = sizeof(long);

  struct utsname sysinfo;
  // If user space is 32-bit check the kernel to verify.
  if (commit_size < 8 && uname(&sysinfo) == 0) {
    // Arm returns armv# for its machine type. The first (and only currently)
    // arm processor that supports 64bit is the armv8 series.
    commit_size =
        strstr(sysinfo.machine, "64") || strstr(sysinfo.machine, "armv8") ? 8
                                                                          : 4;
  }
#endif

  // header_page typically looks as follows on a 64-bit kernel:
  // field: u64 timestamp; offset:0; size:8; signed:0;
  // field: local_t commit; offset:8; size:8; signed:1;
  // field: int overwrite; offset:8; size:1; signed:1;
  // field: char data; offset:16; size:4080; signed:0;
  //
  // On a 32-bit kernel local_t is 32-bit wide and data starts @ offset 12.

  spec.timestamp = FtraceEvent::Field{"u64 timestamp", 0, 8, 0};
  spec.size = FtraceEvent::Field{"local_t commit", 8, commit_size, 1};
  spec.overwrite = FtraceEvent::Field{"int overwrite", 8, 1, 1};
  return spec;
}

const std::vector<Event> BuildEventsVector(const std::vector<Event>& events) {
  size_t largest_id = 0;
  for (const Event& event : events) {
    if (event.ftrace_event_id > largest_id)
      largest_id = event.ftrace_event_id;
  }
  std::vector<Event> events_by_id;
  events_by_id.resize(largest_id + 1);
  for (const Event& event : events) {
    events_by_id[event.ftrace_event_id] = event;
  }
  events_by_id.shrink_to_fit();
  return events_by_id;
}

// Merge the information from |ftrace_field| into |field| (mutating it).
// We should set the following fields: offset, size, ftrace field type and
// translation strategy.
bool MergeFieldInfo(const FtraceEvent::Field& ftrace_field,
                    Field* field,
                    const char* event_name_for_debug) {
  PERFETTO_DCHECK(field->ftrace_name);
  PERFETTO_DCHECK(field->proto_field_id);
  PERFETTO_DCHECK(static_cast<int>(field->proto_field_type));
  PERFETTO_DCHECK(!field->ftrace_offset);
  PERFETTO_DCHECK(!field->ftrace_size);
  PERFETTO_DCHECK(!field->ftrace_type);

  if (!InferFtraceType(ftrace_field.type_and_name, ftrace_field.size,
                       ftrace_field.is_signed, &field->ftrace_type)) {
    PERFETTO_FATAL(
        "Failed to infer ftrace field type for \"%s.%s\" (type:\"%s\" "
        "size:%d "
        "signed:%d)",
        event_name_for_debug, field->ftrace_name,
        ftrace_field.type_and_name.c_str(), ftrace_field.size,
        ftrace_field.is_signed);
    return false;
  }

  field->ftrace_offset = ftrace_field.offset;
  field->ftrace_size = ftrace_field.size;

  if (!SetTranslationStrategy(field->ftrace_type, field->proto_field_type,
                              &field->strategy)) {
    PERFETTO_DLOG(
        "Failed to find translation strategy for ftrace field \"%s.%s\" (%s -> "
        "%s)",
        event_name_for_debug, field->ftrace_name, ToString(field->ftrace_type),
        protozero::proto_utils::ProtoSchemaToString(field->proto_field_type));
    // TODO(hjd): Uncomment DCHECK when proto generation is fixed.
    // PERFETTO_DFATAL("Failed to find translation strategy");
    return false;
  }

  return true;
}

// For each field in |fields| find the matching field from |ftrace_fields| (by
// comparing ftrace_name) and copy the information from the FtraceEvent::Field
// into the Field (mutating it). If there is no matching field in
// |ftrace_fields| remove the Field from |fields|. Return the maximum observed
// 'field end' (offset + size).
uint16_t MergeFields(const std::vector<FtraceEvent::Field>& ftrace_fields,
                     std::vector<Field>* fields,
                     const char* event_name_for_debug) {
  uint16_t fields_end = 0;

  // Loop over each Field in |fields| modifying it with information from the
  // matching |ftrace_fields| field or removing it.
  auto field = fields->begin();
  while (field != fields->end()) {
    bool success = false;
    for (const FtraceEvent::Field& ftrace_field : ftrace_fields) {
      if (GetNameFromTypeAndName(ftrace_field.type_and_name) !=
          field->ftrace_name)
        continue;

      success = MergeFieldInfo(ftrace_field, &*field, event_name_for_debug);

      uint16_t field_end = field->ftrace_offset + field->ftrace_size;
      fields_end = std::max<uint16_t>(fields_end, field_end);

      break;
    }
    if (success) {
      ++field;
    } else {
      field = fields->erase(field);
    }
  }
  return fields_end;
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

std::string RegexError(int errcode, const regex_t* preg) {
  char buf[64];
  regerror(errcode, preg, buf, sizeof(buf));
  return {buf, sizeof(buf)};
}

bool Match(const char* string, const char* pattern) {
  regex_t re;
  int ret = regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB);
  if (ret != 0) {
    PERFETTO_FATAL("regcomp: %s", RegexError(ret, &re).c_str());
  }
  ret = regexec(&re, string, 0, nullptr, 0);
  regfree(&re);
  return ret != REG_NOMATCH;
}

// Set proto field type and id based on the ftrace type.
void SetProtoType(FtraceFieldType ftrace_type,
                  ProtoSchemaType* proto_type,
                  uint32_t* proto_field_id) {
  switch (ftrace_type) {
    case kFtraceCString:
    case kFtraceFixedCString:
    case kFtraceStringPtr:
    case kFtraceDataLoc:
      *proto_type = ProtoSchemaType::kString;
      *proto_field_id = GenericFtraceEvent::Field::kStrValueFieldNumber;
      break;
    case kFtraceInt8:
    case kFtraceInt16:
    case kFtraceInt32:
    case kFtracePid32:
    case kFtraceCommonPid32:
    case kFtraceInt64:
      *proto_type = ProtoSchemaType::kInt64;
      *proto_field_id = GenericFtraceEvent::Field::kIntValueFieldNumber;
      break;
    case kFtraceUint8:
    case kFtraceUint16:
    case kFtraceUint32:
    case kFtraceBool:
    case kFtraceDevId32:
    case kFtraceDevId64:
    case kFtraceUint64:
    case kFtraceInode32:
    case kFtraceInode64:
      *proto_type = ProtoSchemaType::kUint64;
      *proto_field_id = GenericFtraceEvent::Field::kUintValueFieldNumber;
      break;
    case kInvalidFtraceFieldType:
      PERFETTO_FATAL("Unexpected ftrace field type");
  }
}

}  // namespace

// This is similar but different from InferProtoType (see format_parser.cc).
// TODO(hjd): Fold FtraceEvent(::Field) into Event.
bool InferFtraceType(const std::string& type_and_name,
                     size_t size,
                     bool is_signed,
                     FtraceFieldType* out) {
  // Fixed length strings: e.g. "char foo[16]" we don't care about the number
  // since we get the size as it's own field. Somewhat awkwardly these fields
  // are both fixed size and null terminated meaning that we can't just drop
  // them directly into the protobuf (since if the string is shorter than 15
  // characters we want only the bit up to the null terminator).
  if (Match(type_and_name.c_str(), R"(char [a-zA-Z_]+\[[0-9]+\])")) {
    *out = kFtraceFixedCString;
    return true;
  }

  // String pointers: "__data_loc char[] foo" (as in
  // 'cpufreq_interactive_boost').
  // TODO(fmayer): Handle u32[], u8[], __u8[] as well.
  if (Contains(type_and_name, "__data_loc char[] ")) {
    if (size != 4) {
      PERFETTO_ELOG("__data_loc with incorrect size: %s (%zd)",
                    type_and_name.c_str(), size);
      return false;
    }
    *out = kFtraceDataLoc;
    return true;
  }

  if (Contains(type_and_name, "char[] ")) {
    *out = kFtraceStringPtr;
    return true;
  }
  if (Contains(type_and_name, "char * ")) {
    *out = kFtraceStringPtr;
    return true;
  }

  // Variable length strings: "char foo" + size: 0 (as in 'print').
  if (base::StartsWith(type_and_name, "char ") && size == 0) {
    *out = kFtraceCString;
    return true;
  }

  if (base::StartsWith(type_and_name, "bool ")) {
    *out = kFtraceBool;
    return true;
  }

  if (base::StartsWith(type_and_name, "ino_t ") ||
      base::StartsWith(type_and_name, "i_ino ")) {
    if (size == 4) {
      *out = kFtraceInode32;
      return true;
    } else if (size == 8) {
      *out = kFtraceInode64;
      return true;
    }
  }

  if (base::StartsWith(type_and_name, "dev_t ")) {
    if (size == 4) {
      *out = kFtraceDevId32;
      return true;
    } else if (size == 8) {
      *out = kFtraceDevId64;
      return true;
    }
  }

  // Pids (as in 'sched_switch').
  if (base::StartsWith(type_and_name, "pid_t ") && size == 4) {
    *out = kFtracePid32;
    return true;
  }

  if (Contains(type_and_name, "common_pid") && size == 4) {
    *out = kFtraceCommonPid32;
    return true;
  }

  // Ints of various sizes:
  if (size == 1 && is_signed) {
    *out = kFtraceInt8;
    return true;
  } else if (size == 1 && !is_signed) {
    *out = kFtraceUint8;
    return true;
  } else if (size == 2 && is_signed) {
    *out = kFtraceInt16;
    return true;
  } else if (size == 2 && !is_signed) {
    *out = kFtraceUint16;
    return true;
  } else if (size == 4 && is_signed) {
    *out = kFtraceInt32;
    return true;
  } else if (size == 4 && !is_signed) {
    *out = kFtraceUint32;
    return true;
  } else if (size == 8 && is_signed) {
    *out = kFtraceInt64;
    return true;
  } else if (size == 8 && !is_signed) {
    *out = kFtraceUint64;
    return true;
  }

  PERFETTO_DLOG("Could not infer ftrace type for '%s'", type_and_name.c_str());
  return false;
}

// static
ProtoTranslationTable::FtracePageHeaderSpec
ProtoTranslationTable::DefaultPageHeaderSpecForTesting() {
  std::string page_header =
      R"(	field: u64 timestamp;	offset:0;	size:8;	signed:0;
	field: local_t commit;	offset:8;	size:8;	signed:1;
	field: int overwrite;	offset:8;	size:1;	signed:1;
	field: char data;	offset:16;	size:4080;	signed:0;)";
  std::vector<FtraceEvent::Field> page_header_fields;
  PERFETTO_CHECK(ParseFtraceEventBody(std::move(page_header), nullptr,
                                      &page_header_fields));
  return MakeFtracePageHeaderSpec(page_header_fields);
}

// static
std::unique_ptr<ProtoTranslationTable> ProtoTranslationTable::Create(
    const FtraceProcfs* ftrace_procfs,
    std::vector<Event> events,
    std::vector<Field> common_fields) {
  bool common_fields_processed = false;
  uint16_t common_fields_end = 0;

  std::string page_header = ftrace_procfs->ReadPageHeaderFormat();
  bool ftrace_header_parsed = false;
  FtracePageHeaderSpec header_spec{};
  if (!page_header.empty()) {
    std::vector<FtraceEvent::Field> page_header_fields;
    ftrace_header_parsed = ParseFtraceEventBody(std::move(page_header), nullptr,
                                                &page_header_fields);
    header_spec = MakeFtracePageHeaderSpec(page_header_fields);
  }

  if (!ftrace_header_parsed) {
    PERFETTO_LOG("Failed to parse ftrace page header, using fallback layout");
    header_spec = GuessFtracePageHeaderSpec();
  }

  for (Event& event : events) {
    if (event.proto_field_id ==
        protos::pbzero::FtraceEvent::kGenericFieldNumber) {
      continue;
    }
    PERFETTO_DCHECK(event.name);
    PERFETTO_DCHECK(event.group);
    PERFETTO_DCHECK(event.proto_field_id);
    PERFETTO_DCHECK(!event.ftrace_event_id);

    std::string contents =
        ftrace_procfs->ReadEventFormat(event.group, event.name);
    FtraceEvent ftrace_event;
    if (contents.empty() || !ParseFtraceEvent(contents, &ftrace_event)) {
      if (!strcmp(event.group, "ftrace") && !strcmp(event.name, "print")) {
        // On some "user" builds of Android <P the ftrace/print event is not
        // selinux-whitelisted. Thankfully this event is an always-on built-in
        // so we don't need to write to its 'enable' file. However we need to
        // know its binary layout to decode it, so we hardcode it.
        ftrace_event.id = 5;  // Seems quite stable across kernels.
        ftrace_event.name = "print";
        // The only field we care about is:
        // field:char buf; offset:16; size:0; signed:0;
        ftrace_event.fields.emplace_back(
            FtraceEvent::Field{"char buf", 16, 0, 0});
      } else {
        continue;
      }
    }

    event.ftrace_event_id = ftrace_event.id;

    if (!common_fields_processed) {
      common_fields_end =
          MergeFields(ftrace_event.common_fields, &common_fields, event.name);
      common_fields_processed = true;
    }

    uint16_t fields_end =
        MergeFields(ftrace_event.fields, &event.fields, event.name);

    event.size = std::max<uint16_t>(fields_end, common_fields_end);
  }

  events.erase(std::remove_if(events.begin(), events.end(),
                              [](const Event& event) {
                                return event.proto_field_id == 0 ||
                                       event.ftrace_event_id == 0;
                              }),
               events.end());

  // Pre-parse certain scheduler events, and see if the compile-time assumptions
  // about their format hold for this kernel.
  CompactSchedEventFormat compact_sched = ValidateFormatForCompactSched(events);

  auto table = std::unique_ptr<ProtoTranslationTable>(
      new ProtoTranslationTable(ftrace_procfs, events, std::move(common_fields),
                                header_spec, compact_sched));
  return table;
}

ProtoTranslationTable::ProtoTranslationTable(
    const FtraceProcfs* ftrace_procfs,
    const std::vector<Event>& events,
    std::vector<Field> common_fields,
    FtracePageHeaderSpec ftrace_page_header_spec,
    CompactSchedEventFormat compact_sched_format)
    : ftrace_procfs_(ftrace_procfs),
      events_(BuildEventsVector(events)),
      largest_id_(events_.size() - 1),
      common_fields_(std::move(common_fields)),
      ftrace_page_header_spec_(ftrace_page_header_spec),
      compact_sched_format_(compact_sched_format) {
  for (const Event& event : events) {
    group_and_name_to_event_[GroupAndName(event.group, event.name)] =
        &events_.at(event.ftrace_event_id);
    name_to_events_[event.name].push_back(&events_.at(event.ftrace_event_id));
    group_to_events_[event.group].push_back(&events_.at(event.ftrace_event_id));
  }
}

const Event* ProtoTranslationTable::GetOrCreateEvent(
    const GroupAndName& group_and_name) {
  const Event* event = GetEvent(group_and_name);
  if (event)
    return event;
  // The ftrace event does not already exist so a new one will be created
  // by parsing the format file.
  std::string contents = ftrace_procfs_->ReadEventFormat(group_and_name.group(),
                                                         group_and_name.name());
  if (contents.empty())
    return nullptr;
  FtraceEvent ftrace_event = {};
  ParseFtraceEvent(contents, &ftrace_event);

  // Ensure events vector is large enough
  if (ftrace_event.id > largest_id_) {
    events_.resize(ftrace_event.id + 1);
    largest_id_ = ftrace_event.id;
  }

  // Set known event variables
  Event* e = &events_.at(ftrace_event.id);
  e->ftrace_event_id = ftrace_event.id;
  e->proto_field_id = protos::pbzero::FtraceEvent::kGenericFieldNumber;
  e->name = InternString(group_and_name.name());
  e->group = InternString(group_and_name.group());

  // Calculate size of common fields.
  for (const FtraceEvent::Field& ftrace_field : ftrace_event.common_fields) {
    uint16_t field_end = ftrace_field.offset + ftrace_field.size;
    e->size = std::max(field_end, e->size);
  }

  // For every field in the ftrace event, make a field in the generic event.
  for (const FtraceEvent::Field& ftrace_field : ftrace_event.fields)
    e->size = std::max(CreateGenericEventField(ftrace_field, *e), e->size);

  group_and_name_to_event_[group_and_name] = &events_.at(e->ftrace_event_id);
  name_to_events_[e->name].push_back(&events_.at(e->ftrace_event_id));
  group_to_events_[e->group].push_back(&events_.at(e->ftrace_event_id));

  return e;
}

const char* ProtoTranslationTable::InternString(const std::string& str) {
  auto it_and_inserted = interned_strings_.insert(str);
  return it_and_inserted.first->c_str();
}

uint16_t ProtoTranslationTable::CreateGenericEventField(
    const FtraceEvent::Field& ftrace_field,
    Event& event) {
  uint16_t field_end = ftrace_field.offset + ftrace_field.size;
  std::string field_name = GetNameFromTypeAndName(ftrace_field.type_and_name);
  if (field_name.empty()) {
    PERFETTO_DLOG("Field: %s could not be added to the generic event.",
                  ftrace_field.type_and_name.c_str());
    return field_end;
  }
  event.fields.emplace_back();
  Field* field = &event.fields.back();
  field->ftrace_name = InternString(field_name);
  if (!InferFtraceType(ftrace_field.type_and_name, ftrace_field.size,
                       ftrace_field.is_signed, &field->ftrace_type)) {
    PERFETTO_DLOG(
        "Failed to infer ftrace field type for \"%s.%s\" (type:\"%s\" "
        "size:%d "
        "signed:%d)",
        event.name, field->ftrace_name, ftrace_field.type_and_name.c_str(),
        ftrace_field.size, ftrace_field.is_signed);
    event.fields.pop_back();
    return field_end;
  }
  SetProtoType(field->ftrace_type, &field->proto_field_type,
               &field->proto_field_id);
  field->ftrace_offset = ftrace_field.offset;
  field->ftrace_size = ftrace_field.size;
  // Proto type is set based on ftrace type so all fields should have a
  // translation strategy.
  bool success = SetTranslationStrategy(
      field->ftrace_type, field->proto_field_type, &field->strategy);
  PERFETTO_DCHECK(success);
  return field_end;
}

EventFilter::EventFilter() = default;
EventFilter::~EventFilter() = default;

void EventFilter::AddEnabledEvent(size_t ftrace_event_id) {
  if (ftrace_event_id >= enabled_ids_.size())
    enabled_ids_.resize(ftrace_event_id + 1);
  enabled_ids_[ftrace_event_id] = true;
}

void EventFilter::DisableEvent(size_t ftrace_event_id) {
  if (ftrace_event_id >= enabled_ids_.size())
    return;
  enabled_ids_[ftrace_event_id] = false;
}

bool EventFilter::IsEventEnabled(size_t ftrace_event_id) const {
  if (ftrace_event_id == 0 || ftrace_event_id >= enabled_ids_.size())
    return false;
  return enabled_ids_[ftrace_event_id];
}

std::set<size_t> EventFilter::GetEnabledEvents() const {
  std::set<size_t> enabled;
  for (size_t i = 0; i < enabled_ids_.size(); i++) {
    if (enabled_ids_[i]) {
      enabled.insert(i);
    }
  }
  return enabled;
}

void EventFilter::EnableEventsFrom(const EventFilter& other) {
  size_t max_length = std::max(enabled_ids_.size(), other.enabled_ids_.size());
  enabled_ids_.resize(max_length);
  for (size_t i = 0; i < other.enabled_ids_.size(); i++) {
    if (other.enabled_ids_[i])
      enabled_ids_[i] = true;
  }
}

ProtoTranslationTable::~ProtoTranslationTable() = default;

}  // namespace perfetto
