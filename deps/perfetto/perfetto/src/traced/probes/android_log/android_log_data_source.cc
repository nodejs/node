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

#include "src/traced/probes/android_log/android_log_data_source.h"

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/data_source_config.h"

#include "protos/perfetto/common/android_log_constants.pbzero.h"
#include "protos/perfetto/config/android/android_log_config.pbzero.h"
#include "protos/perfetto/trace/android/android_log.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

namespace {

using protos::pbzero::AndroidLogConfig;
using protos::pbzero::AndroidLogId;

constexpr size_t kBufSize = base::kPageSize;
const char kLogTagsPath[] = "/system/etc/event-log-tags";
const char kLogdrSocket[] = "/dev/socket/logdr";

// From AOSP's liblog/include/log/log_read.h .
// Android Log is supposed to be an ABI as it's exposed also by logcat -b (and
// in practice never changed in backwards-incompatible ways in the past).
// Note: the casing doesn't match the style guide and instead matches the
// original name in liblog for the sake of code-searcheability.
struct logger_entry_v4 {
  uint16_t len;       // length of the payload.
  uint16_t hdr_size;  // sizeof(struct logger_entry_v4).
  int32_t pid;        // generating process's pid.
  uint32_t tid;       // generating process's tid.
  uint32_t sec;       // seconds since Epoch.
  uint32_t nsec;      // nanoseconds.
  uint32_t lid;       // log id of the payload, bottom 4 bits currently.
  uint32_t uid;       // generating process's uid.
};

// Event types definition in the binary encoded format, from
// liblog/include/log/log.h .
// Note that these values don't match the textual definitions of
// system/core/android_logevent.logtags (which are not used by perfetto).
// The latter are off by one (INT = 1, LONG=2 and so on).
enum AndroidEventLogType {
  EVENT_TYPE_INT = 0,
  EVENT_TYPE_LONG = 1,
  EVENT_TYPE_STRING = 2,
  EVENT_TYPE_LIST = 3,
  EVENT_TYPE_FLOAT = 4,
};

template <typename T>
inline bool ReadAndAdvance(const char** ptr, const char* end, T* out) {
  if (*ptr > end - sizeof(T))
    return false;
  memcpy(reinterpret_cast<void*>(out), *ptr, sizeof(T));
  *ptr += sizeof(T);
  return true;
}

}  // namespace

// static
const ProbesDataSource::Descriptor AndroidLogDataSource::descriptor = {
    /*name*/ "android.log",
    /*flags*/ Descriptor::kFlagsNone,
};

AndroidLogDataSource::AndroidLogDataSource(DataSourceConfig ds_config,
                                           base::TaskRunner* task_runner,
                                           TracingSessionID session_id,
                                           std::unique_ptr<TraceWriter> writer)
    : ProbesDataSource(session_id, &descriptor),
      task_runner_(task_runner),
      writer_(std::move(writer)),
      weak_factory_(this) {
  AndroidLogConfig::Decoder cfg(ds_config.android_log_config_raw());

  std::vector<uint32_t> log_ids;
  for (auto id = cfg.log_ids(); id; ++id)
    log_ids.push_back(static_cast<uint32_t>(*id));

  if (log_ids.empty()) {
    // If no log id is specified, add the most popular ones.
    log_ids.push_back(AndroidLogId::LID_DEFAULT);
    log_ids.push_back(AndroidLogId::LID_EVENTS);
    log_ids.push_back(AndroidLogId::LID_SYSTEM);
    log_ids.push_back(AndroidLogId::LID_CRASH);
    log_ids.push_back(AndroidLogId::LID_KERNEL);
  }

  // Build the command string that will be sent to the logdr socket on Start(),
  // which looks like "stream lids=1,2,3,4" (lids == log buffer id(s)).
  mode_ = "stream tail=1 lids";
  for (auto it = log_ids.begin(); it != log_ids.end(); it++) {
    mode_ += it == log_ids.begin() ? "=" : ",";
    mode_ += std::to_string(*it);
  }

  // Build a linear vector out of the tag filters and keep track of the string
  // boundaries. Once done, derive StringView(s) out of the vector. This is
  // to create a set<StringView> which is backed by contiguous chars that can be
  // used to lookup StringView(s) from the parsed buffer.
  // This is to avoid copying strings of tags for the only sake of checking for
  // their existence in the set.
  std::vector<std::pair<size_t, size_t>> tag_boundaries;
  for (auto it = cfg.filter_tags(); it; ++it) {
    base::StringView tag(*it);
    const size_t begin = filter_tags_strbuf_.size();
    filter_tags_strbuf_.insert(filter_tags_strbuf_.end(), tag.begin(),
                               tag.end());
    const size_t end = filter_tags_strbuf_.size();
    tag_boundaries.emplace_back(begin, end - begin);
  }
  filter_tags_strbuf_.shrink_to_fit();
  // At this point pointers to |filter_tags_strbuf_| are stable.
  for (const auto& it : tag_boundaries)
    filter_tags_.emplace(&filter_tags_strbuf_[it.first], it.second);

  min_prio_ = cfg.min_prio();
  buf_ = base::PagedMemory::Allocate(kBufSize);
}

AndroidLogDataSource::~AndroidLogDataSource() {
  if (logdr_sock_) {
    EnableSocketWatchTask(false);
    logdr_sock_.Shutdown();
  }
}

base::UnixSocketRaw AndroidLogDataSource::ConnectLogdrSocket() {
  auto socket = base::UnixSocketRaw::CreateMayFail(base::SockFamily::kUnix,
                                                   base::SockType::kSeqPacket);
  if (!socket || !socket.Connect(kLogdrSocket)) {
    PERFETTO_PLOG("Failed to connect to %s", kLogdrSocket);
    return base::UnixSocketRaw();
  }
  return socket;
}

void AndroidLogDataSource::Start() {
  ParseEventLogDefinitions();

  if (!(logdr_sock_ = ConnectLogdrSocket()))
    return;

  PERFETTO_DLOG("Starting Android log data source: %s", mode_.c_str());
  if (logdr_sock_.Send(mode_.data(), mode_.size()) <= 0) {
    PERFETTO_PLOG("send() failed on logdr socket %s", kLogdrSocket);
    return;
  }
  logdr_sock_.SetBlocking(false);
  EnableSocketWatchTask(true);
}

void AndroidLogDataSource::EnableSocketWatchTask(bool enable) {
  if (fd_watch_task_enabled_ == enable)
    return;

  if (enable) {
    auto weak_this = weak_factory_.GetWeakPtr();
    task_runner_->AddFileDescriptorWatch(logdr_sock_.fd(), [weak_this] {
      if (weak_this)
        weak_this->OnSocketDataAvailable();
    });
  } else {
    task_runner_->RemoveFileDescriptorWatch(logdr_sock_.fd());
  }

  fd_watch_task_enabled_ = enable;
}

void AndroidLogDataSource::OnSocketDataAvailable() {
  PERFETTO_DCHECK(fd_watch_task_enabled_);
  auto now_ms = base::GetWallTimeMs().count();

  // Disable the FD watch until the delayed read happens, otherwise we get a
  // storm of OnSocketDataAvailable() until the delayed ReadLogSocket() happens.
  EnableSocketWatchTask(false);

  // Delay the read by (at most) 100 ms so we get a chance to batch reads and
  // don't cause too many context switches in cases of logging storms. The
  // modulo is to increase the chance that the wakeup is packed together with
  // some other wakeup task of traced_probes.
  const uint32_t kBatchMs = 100;
  uint32_t delay_ms = kBatchMs - (now_ms % kBatchMs);
  auto weak_this = weak_factory_.GetWeakPtr();
  task_runner_->PostDelayedTask(
      [weak_this] {
        if (weak_this) {
          weak_this->ReadLogSocket();
          weak_this->EnableSocketWatchTask(true);
        }
      },
      delay_ms);
}

void AndroidLogDataSource::ReadLogSocket() {
  TraceWriter::TracePacketHandle packet;
  protos::pbzero::AndroidLogPacket* log_packet = nullptr;
  size_t num_events = 0;
  bool stop = false;
  ssize_t rsize;
  while (!stop && (rsize = logdr_sock_.Receive(buf_.Get(), kBufSize)) > 0) {
    num_events++;
    stats_.num_total++;
    // Don't hold the message loop for too long. If there are so many events
    // in the queue, stop at some point and parse the remaining ones in another
    // task (posted after this while loop).
    if (num_events > 500) {
      stop = true;
      auto weak_this = weak_factory_.GetWeakPtr();
      task_runner_->PostTask([weak_this] {
        if (weak_this)
          weak_this->ReadLogSocket();
      });
    }
    char* buf = reinterpret_cast<char*>(buf_.Get());
    PERFETTO_DCHECK(reinterpret_cast<uintptr_t>(buf) % 16 == 0);
    size_t payload_size = reinterpret_cast<logger_entry_v4*>(buf)->len;
    size_t hdr_size = reinterpret_cast<logger_entry_v4*>(buf)->hdr_size;
    if (payload_size + hdr_size > static_cast<size_t>(rsize)) {
      PERFETTO_DLOG(
          "Invalid Android log frame (hdr: %zu, payload: %zu, rsize: %zd)",
          hdr_size, payload_size, rsize);
      stats_.num_failed++;
      continue;
    }
    char* const end = buf + hdr_size + payload_size;

    // In older versions of Android the logger_entry struct can contain less
    // fields. Copy that in a temporary struct, so that unset fields are
    // always zero-initialized.
    logger_entry_v4 entry{};
    memcpy(&entry, buf, std::min(hdr_size, sizeof(entry)));
    buf += hdr_size;

    if (!packet) {
      // Lazily add the packet on the first event. This is to avoid creating
      // empty packets if there are no events in a task.
      packet = writer_->NewTracePacket();
      packet->set_timestamp(
          static_cast<uint64_t>(base::GetBootTimeNs().count()));
      log_packet = packet->set_android_log();
    }

    protos::pbzero::AndroidLogPacket::LogEvent* evt = nullptr;

    if (entry.lid == AndroidLogId::LID_EVENTS) {
      // Entries in the EVENTS buffer are special, they are binary encoded.
      // See https://developer.android.com/reference/android/util/EventLog.
      if (!ParseBinaryEvent(buf, end, log_packet, &evt)) {
        PERFETTO_DLOG("Failed to parse Android log binary event");
        stats_.num_failed++;
        continue;
      }
    } else {
      if (!ParseTextEvent(buf, end, log_packet, &evt)) {
        PERFETTO_DLOG("Failed to parse Android log text event");
        stats_.num_failed++;
        continue;
      }
    }
    if (!evt) {
      // Parsing succeeded but the event was skipped due to filters.
      stats_.num_skipped++;
      continue;
    }

    // Add the common fields to the event.
    uint64_t ts = entry.sec * 1000000000ULL + entry.nsec;
    evt->set_timestamp(ts);
    evt->set_log_id(static_cast<protos::pbzero::AndroidLogId>(entry.lid));
    evt->set_pid(entry.pid);
    evt->set_tid(static_cast<int32_t>(entry.tid));
    evt->set_uid(static_cast<int32_t>(entry.uid));
  }  // while(logdr_sock_.Receive())

  // Only print the log message if we have seen a bunch of events. This is to
  // avoid that we keep re-triggering the log socket by writing into the log
  // buffer ourselves.
  if (num_events > 3)
    PERFETTO_DLOG("Seen %zu Android log events", num_events);
}

bool AndroidLogDataSource::ParseTextEvent(
    const char* start,
    const char* end,
    protos::pbzero::AndroidLogPacket* packet,
    protos::pbzero::AndroidLogPacket::LogEvent** out_evt) {
  // Format: [Priority 1 byte] [ tag ] [ NUL ] [ message ]
  const char* buf = start;
  int8_t prio;
  if (!ReadAndAdvance(&buf, end, &prio))
    return false;

  if (prio > 10) {
    PERFETTO_DLOG("Skipping log event with suspiciously high priority %d",
                  prio);
    return false;
  }

  // Skip if the user specified a min-priority filter in the config.
  if (prio < min_prio_)
    return true;

  // Find the null terminator that separates |tag| from |message|.
  const char* str_end;
  for (str_end = buf; str_end < end && *str_end; str_end++) {
  }
  if (str_end >= end - 2)
    return false;

  auto tag = base::StringView(buf, static_cast<size_t>(str_end - buf));
  if (!filter_tags_.empty() && filter_tags_.count(tag) == 0)
    return true;

  auto* evt = packet->add_events();
  *out_evt = evt;
  evt->set_prio(static_cast<protos::pbzero::AndroidLogPriority>(prio));
  evt->set_tag(tag.data(), tag.size());

  buf = str_end + 1;  // Move |buf| to the start of the message.
  size_t msg_len = static_cast<size_t>(end - buf);

  // Protobuf strings don't need the null terminator. If the string is
  // null terminated, omit the terminator.
  if (msg_len > 0 && *(end - 1) == '\0')
    msg_len--;

  evt->set_message(buf, msg_len);
  return true;
}

bool AndroidLogDataSource::ParseBinaryEvent(
    const char* start,
    const char* end,
    protos::pbzero::AndroidLogPacket* packet,
    protos::pbzero::AndroidLogPacket::LogEvent** out_evt) {
  const char* buf = start;
  int32_t eid;
  if (!ReadAndAdvance(&buf, end, &eid))
    return false;

  // TODO test events with 0 arguments. DNS.

  const EventFormat* fmt = GetEventFormat(eid);
  if (!fmt) {
    // We got an event which doesn't have a corresponding entry in
    // /system/etc/event-log-tags. In most cases this is a bug in the App that
    // produced the event, which forgot to update the log tags dictionary.
    return false;
  }

  if (!filter_tags_.empty() &&
      filter_tags_.count(base::StringView(fmt->name)) == 0) {
    return true;
  }

  auto* evt = packet->add_events();
  *out_evt = evt;
  evt->set_tag(fmt->name.c_str());
  size_t field_num = 0;
  while (buf < end) {
    char type = *(buf++);
    if (field_num >= fmt->fields.size())
      return true;
    const char* field_name = fmt->fields[field_num].c_str();
    switch (type) {
      case EVENT_TYPE_INT: {
        int32_t value;
        if (!ReadAndAdvance(&buf, end, &value))
          return false;
        auto* arg = evt->add_args();
        arg->set_name(field_name);
        arg->set_int_value(value);
        field_num++;
        break;
      }
      case EVENT_TYPE_LONG: {
        int64_t value;
        if (!ReadAndAdvance(&buf, end, &value))
          return false;
        auto* arg = evt->add_args();
        arg->set_name(field_name);
        arg->set_int_value(value);
        field_num++;
        break;
      }
      case EVENT_TYPE_FLOAT: {
        float value;
        if (!ReadAndAdvance(&buf, end, &value))
          return false;
        auto* arg = evt->add_args();
        arg->set_name(field_name);
        arg->set_float_value(value);
        field_num++;
        break;
      }
      case EVENT_TYPE_STRING: {
        uint32_t len;
        if (!ReadAndAdvance(&buf, end, &len) || buf + len > end)
          return false;
        auto* arg = evt->add_args();
        arg->set_name(field_name);
        arg->set_string_value(buf, len);
        buf += len;
        field_num++;
        break;
      }
      case EVENT_TYPE_LIST: {
        buf++;  // EVENT_TYPE_LIST has one byte payload containing the list len.
        if (field_num > 0) {
          // Lists are supported only as a top-level node. We stop parsing when
          // encountering a list as an inner field. The very few of them are not
          // interesting enough to warrant the extra complexity.
          return true;
        }
        break;
      }
      default:
        PERFETTO_DLOG(
            "Skipping unknown Android log binary event of type %d for %s at pos"
            " %zd after parsing %zu fields",
            static_cast<int>(type), fmt->name.c_str(), buf - start, field_num);
        return true;
    }  // switch(type)
  }    // while(buf < end)
  return true;
}

void AndroidLogDataSource::Flush(FlushRequestID,
                                 std::function<void()> callback) {
  // Grab most recent entries.
  if (logdr_sock_)
    ReadLogSocket();

  // Emit stats.
  {
    auto packet = writer_->NewTracePacket();
    packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));
    auto* stats = packet->set_android_log()->set_stats();
    stats->set_num_total(stats_.num_total);
    stats->set_num_skipped(stats_.num_skipped);
    stats->set_num_failed(stats_.num_failed);
  }

  writer_->Flush(callback);
}

void AndroidLogDataSource::ParseEventLogDefinitions() {
  std::string event_log_tags = ReadEventLogDefinitions();
  for (base::StringSplitter ss(std::move(event_log_tags), '\n'); ss.Next();) {
    if (!ParseEventLogDefinitionLine(ss.cur_token(), ss.cur_token_size() + 1)) {
      PERFETTO_DLOG("Could not parse event log format: %s", ss.cur_token());
    }
  }
}

bool AndroidLogDataSource::ParseEventLogDefinitionLine(char* line, size_t len) {
  base::StringSplitter tok(line, len, ' ');
  if (!tok.Next())
    return false;
  auto id = static_cast<uint32_t>(std::strtoul(tok.cur_token(), nullptr, 10));
  if (!tok.Next())
    return false;
  std::string name(tok.cur_token(), tok.cur_token_size());
  auto it = event_formats_.emplace(id, EventFormat{std::move(name), {}}).first;
  char* format = tok.cur_token() + tok.cur_token_size() + 1;
  if (format >= line + len || !*format || *format == '\n') {
    return true;
  }
  size_t format_len = len - static_cast<size_t>(format - line);

  // Parse the arg formats, e.g.:
  // (status|1|5),(health|1|5),(present|1|5),(plugged|1|5),(technology|3).
  // We don't really care neither about the field type nor its unit (the two
  // numbers after the |). The binary format re-states the type and we don't
  // currently propagate the unit at all.
  bool parsed_all_tokens = true;
  for (base::StringSplitter field(format, format_len, ','); field.Next();) {
    if (field.cur_token_size() <= 2) {
      parsed_all_tokens = false;
      continue;
    }
    char* start = field.cur_token() + 1;
    base::StringSplitter parts(start, field.cur_token_size() - 1, '|');
    if (!parts.Next()) {
      parsed_all_tokens = false;
      continue;
    }
    std::string field_name(parts.cur_token(), parts.cur_token_size());
    it->second.fields.emplace_back(std::move(field_name));
  }
  return parsed_all_tokens;
}

std::string AndroidLogDataSource::ReadEventLogDefinitions() {
  std::string contents;
  if (!base::ReadFile(kLogTagsPath, &contents)) {
    PERFETTO_PLOG("Failed to read %s", kLogTagsPath);
    return "";
  }
  return contents;
}

const AndroidLogDataSource::EventFormat* AndroidLogDataSource::GetEventFormat(
    int id) const {
  auto it = event_formats_.find(id);
  return it == event_formats_.end() ? nullptr : &it->second;
}

}  // namespace perfetto
