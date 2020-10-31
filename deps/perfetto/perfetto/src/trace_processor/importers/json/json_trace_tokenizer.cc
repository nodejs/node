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


#include "src/trace_processor/importers/json/json_trace_tokenizer.h"

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/string_utils.h"

#include "src/trace_processor/importers/json/json_tracker.h"
#include "src/trace_processor/importers/json/json_utils.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/trace_sorter.h"

namespace perfetto {
namespace trace_processor {

namespace {

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
util::Status AppendUnescapedCharacter(char c,
                                      bool is_escaping,
                                      std::string* key) {
  if (is_escaping) {
    switch (c) {
      case '"':
      case '\\':
      case '/':
        key->push_back(c);
        break;
      case 'b':
        key->push_back('\b');
        break;
      case 'f':
        key->push_back('\f');
        break;
      case 'n':
        key->push_back('\n');
        break;
      case 'r':
        key->push_back('\r');
        break;
      case 't':
        key->push_back('\t');
        break;
      default:
        // We don't support any other escape sequences (concretely \uxxxx
        // which JSON supports but is too much effort for us to parse).
        return util::ErrStatus("Illegal character in JSON");
    }
  } else if (c != '\\') {
    key->push_back(c);
  }
  return util::OkStatus();
}

enum class ReadStringRes {
  kEndOfString,
  kNeedsMoreData,
  kFatalError,
};
ReadStringRes ReadOneJsonString(const char* start,
                                const char* end,
                                std::string* key,
                                const char** next) {
  bool is_escaping = false;
  for (const char* s = start; s < end; s++) {
    // Control characters are not allowed in JSON strings.
    if (iscntrl(*s))
      return ReadStringRes::kFatalError;

    // If we get a quote character end of the string.
    if (*s == '"' && !is_escaping) {
      *next = s + 1;
      return ReadStringRes::kEndOfString;
    }

    util::Status status = AppendUnescapedCharacter(*s, is_escaping, key);
    if (!status.ok())
      return ReadStringRes::kFatalError;

    // If we're in a string and we see a backslash and the last character was
    // not a backslash the next character is escaped:
    is_escaping = *s == '\\' && !is_escaping;
  }
  return ReadStringRes::kNeedsMoreData;
}
#endif

}  // namespace

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
ReadDictRes ReadOneJsonDict(const char* start,
                            const char* end,
                            Json::Value* value,
                            const char** next) {
  int braces = 0;
  int square_brackets = 0;
  const char* dict_begin = nullptr;
  bool in_string = false;
  bool is_escaping = false;
  for (const char* s = start; s < end; s++) {
    if (isspace(*s) || *s == ',')
      continue;
    if (*s == '"' && !is_escaping) {
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      // If we're in a string and we see a backslash and the last character was
      // not a backslash the next character is escaped:
      is_escaping = *s == '\\' && !is_escaping;
      // If we're currently parsing a string we should ignore otherwise special
      // characters:
      continue;
    }
    if (*s == '{') {
      if (braces == 0)
        dict_begin = s;
      braces++;
      continue;
    }
    if (*s == '}') {
      if (braces <= 0)
        return ReadDictRes::kEndOfTrace;
      if (--braces > 0)
        continue;
      size_t len = static_cast<size_t>((s + 1) - dict_begin);
      auto opt_value = json::ParseJsonString(base::StringView(dict_begin, len));
      if (!opt_value) {
        PERFETTO_ELOG("Error while parsing JSON string during tokenization");
        return ReadDictRes::kFatalError;
      }
      *value = std::move(*opt_value);
      *next = s + 1;
      return ReadDictRes::kFoundDict;
    }
    if (*s == '[') {
      square_brackets++;
      continue;
    }
    if (*s == ']') {
      if (square_brackets == 0) {
        // We've reached the end of [traceEvents] array.
        // There might be other top level keys in the json (e.g. metadata)
        // after.
        *next = s + 1;
        return ReadDictRes::kEndOfArray;
      }
      square_brackets--;
    }
  }
  return ReadDictRes::kNeedsMoreData;
}

ReadKeyRes ReadOneJsonKey(const char* start,
                          const char* end,
                          std::string* key,
                          const char** next) {
  enum class NextToken {
    kStringOrEndOfDict,
    kColon,
    kValue,
  };

  NextToken next_token = NextToken::kStringOrEndOfDict;
  bool seen_comma = false;
  for (const char* s = start; s < end; s++) {
    // Whitespace characters anywhere can be skipped.
    if (isspace(*s))
      continue;

    switch (next_token) {
      case NextToken::kStringOrEndOfDict: {
        // If we see a closing brace, that means we've reached the end of the
        // wrapping dictionary.
        if (*s == '}') {
          *next = s + 1;
          return ReadKeyRes::kEndOfDictionary;
        }

        // If we see a comma separator, just ignore it.
        if (*s == ',') {
          if (!seen_comma)
            continue;

          seen_comma = true;
          continue;
        }

        // If we see anything else but a quote character here, this cannot be a
        // valid key.
        if (*s != '"')
          return ReadKeyRes::kFatalError;

        auto res = ReadOneJsonString(s + 1, end, key, &s);
        if (res == ReadStringRes::kFatalError)
          return ReadKeyRes::kFatalError;
        if (res == ReadStringRes::kNeedsMoreData)
          return ReadKeyRes::kNeedsMoreData;

        // We need to decrement from the pointer as the loop will increment
        // it back up.
        s--;
        next_token = NextToken::kColon;
        break;
      }
      case NextToken::kColon:
        if (*s != ':')
          return ReadKeyRes::kFatalError;
        next_token = NextToken::kValue;
        break;
      case NextToken::kValue:
        *next = s;
        return ReadKeyRes::kFoundKey;
    }
  }
  return ReadKeyRes::kNeedsMoreData;
}

ReadSystemLineRes ReadOneSystemTraceLine(const char* start,
                                         const char* end,
                                         std::string* line,
                                         const char** next) {
  bool is_escaping = false;
  for (const char* s = start; s < end; s++) {
    // If we get a quote character and we're not escaping, we are done with the
    // system trace string.
    if (*s == '"' && !is_escaping) {
      *next = s + 1;
      return ReadSystemLineRes::kEndOfSystemTrace;
    }

    // If we are escaping n, that means this is a new line which is a delimiter
    // for a system trace line.
    if (*s == 'n' && is_escaping) {
      *next = s + 1;
      return ReadSystemLineRes::kFoundLine;
    }

    util::Status status = AppendUnescapedCharacter(*s, is_escaping, line);
    if (!status.ok())
      return ReadSystemLineRes::kFatalError;

    // If we're in a string and we see a backslash and the last character was
    // not a backslash the next character is escaped:
    is_escaping = *s == '\\' && !is_escaping;
  }
  return ReadSystemLineRes::kNeedsMoreData;
}
#endif

JsonTraceTokenizer::JsonTraceTokenizer(TraceProcessorContext* ctx)
    : context_(ctx) {}
JsonTraceTokenizer::~JsonTraceTokenizer() = default;

util::Status JsonTraceTokenizer::Parse(std::unique_ptr<uint8_t[]> data,
                                       size_t size) {
  PERFETTO_DCHECK(json::IsJsonSupported());

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  buffer_.insert(buffer_.end(), data.get(), data.get() + size);
  const char* buf = buffer_.data();
  const char* next = buf;
  const char* end = buf + buffer_.size();

  JsonTracker* json_tracker = JsonTracker::GetOrCreate(context_);

  // It's possible the displayTimeUnit key is at the end of the json
  // file so to be correct we ought to parse the whole file looking
  // for this key before parsing any events however this would require
  // two passes on the file so for now we only handle displayTimeUnit
  // correctly if it is at the beginning of the file.
  const base::StringView view(buf, size);
  if (view.find("\"displayTimeUnit\":\"ns\"") != base::StringView::npos) {
    json_tracker->SetTimeUnit(json::TimeUnit::kNs);
  } else if (view.find("\"displayTimeUnit\":\"ms\"") !=
             base::StringView::npos) {
    json_tracker->SetTimeUnit(json::TimeUnit::kMs);
  }

  if (offset_ == 0) {
    // Strip leading whitespace.
    while (next != end && isspace(*next)) {
      next++;
    }
    if (next == end) {
      return util::ErrStatus(
          "Failed to parse: first chunk has only whitespace");
    }

    // Trace could begin in any of these ways:
    // {"traceEvents":[{
    // { "traceEvents": [{
    // [{
    if (*next != '{' && *next != '[') {
      return util::ErrStatus(
          "Failed to parse: first non-whitespace character is not [ or {");
    }

    // Figure out the format of the JSON file based on the first non-whitespace
    // character.
    format_ = *next == '{' ? TraceFormat::kOuterDictionary
                           : TraceFormat::kOnlyTraceEvents;

    // Skip the '[' or '{' character.
    next++;

    // Set our current position based on the format of the trace.
    position_ = format_ == TraceFormat::kOuterDictionary
                    ? TracePosition::kDictionaryKey
                    : TracePosition::kTraceEventsArray;
  }

  auto status = ParseInternal(next, end, &next);
  if (!status.ok())
    return status;

  offset_ += static_cast<uint64_t>(next - buf);
  buffer_.erase(buffer_.begin(), buffer_.begin() + (next - buf));
  return util::OkStatus();
#else
  perfetto::base::ignore_result(data);
  perfetto::base::ignore_result(size);
  perfetto::base::ignore_result(context_);
  perfetto::base::ignore_result(format_);
  perfetto::base::ignore_result(position_);
  perfetto::base::ignore_result(offset_);
  return util::ErrStatus("Cannot parse JSON trace due to missing JSON support");
#endif
}

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
util::Status JsonTraceTokenizer::ParseInternal(const char* start,
                                               const char* end,
                                               const char** out) {
  PERFETTO_DCHECK(json::IsJsonSupported());
  JsonTracker* json_tracker = JsonTracker::GetOrCreate(context_);
  auto* trace_sorter = context_->sorter.get();

  const char* next = start;
  switch (position_) {
    case TracePosition::kDictionaryKey: {
      if (format_ != TraceFormat::kOuterDictionary) {
        return util::ErrStatus(
            "Failed to parse: illegal JSON format when parsing dictionary key");
      }

      std::string key;
      auto res = ReadOneJsonKey(start, end, &key, &next);
      if (res == ReadKeyRes::kFatalError)
        return util::ErrStatus("Encountered fatal error while parsing JSON");

      if (res == ReadKeyRes::kEndOfDictionary ||
          res == ReadKeyRes::kNeedsMoreData) {
        break;
      }

      if (key == "traceEvents") {
        position_ = TracePosition::kTraceEventsArray;
        return ParseInternal(next + 1, end, out);
      } else if (key == "systemTraceEvents") {
        position_ = TracePosition::kSystemTraceEventsString;
        return ParseInternal(next + 1, end, out);
      } else if (key == "metadata") {
        position_ = TracePosition::kWaitingForMetadataDictionary;
        return ParseInternal(next + 1, end, out);
      } else {
        // If we don't recognize the key, just ignore the rest of the trace and
        // go to EOF.
        // TODO(lalitm): do something better here.
        position_ = TracePosition::kEof;
        break;
      }
    }
    case TracePosition::kSystemTraceEventsString: {
      if (format_ != TraceFormat::kOuterDictionary) {
        return util::ErrStatus(
            "Failed to parse: illegal JSON format when parsing system events");
      }

      while (next < end) {
        std::string raw_line;
        auto res = ReadOneSystemTraceLine(next, end, &raw_line, &next);
        if (res == ReadSystemLineRes::kFatalError)
          return util::ErrStatus("Encountered fatal error while parsing JSON");

        if (res == ReadSystemLineRes::kNeedsMoreData)
          break;

        if (res == ReadSystemLineRes::kEndOfSystemTrace) {
          position_ = TracePosition::kDictionaryKey;
          return ParseInternal(next, end, out);
        }

        if (base::StartsWith(raw_line, "#") || raw_line.empty())
          continue;

        std::unique_ptr<SystraceLine> line(new SystraceLine());
        util::Status status =
            systrace_line_tokenizer_.Tokenize(raw_line, line.get());
        if (!status.ok())
          return status;
        trace_sorter->PushSystraceLine(std::move(line));
      }
      break;
    }
    case TracePosition::kWaitingForMetadataDictionary: {
      if (format_ != TraceFormat::kOuterDictionary) {
        return util::ErrStatus(
            "Failed to parse: illegal JSON format when parsing metadata");
      }

      std::unique_ptr<Json::Value> value(new Json::Value());
      const auto res = ReadOneJsonDict(next, end, value.get(), &next);
      if (res == ReadDictRes::kFatalError || res == ReadDictRes::kEndOfArray)
        return util::ErrStatus("Encountered fatal error while parsing JSON");
      if (res == ReadDictRes::kEndOfTrace ||
          res == ReadDictRes::kNeedsMoreData) {
        break;
      }

      // TODO(lalitm): read and ingest the relevant data inside |value|.
      position_ = TracePosition::kDictionaryKey;
      break;
    }
    case TracePosition::kTraceEventsArray: {
      while (next < end) {
        std::unique_ptr<Json::Value> value(new Json::Value());
        const auto res = ReadOneJsonDict(next, end, value.get(), &next);
        if (res == ReadDictRes::kFatalError)
          return util::ErrStatus("Encountered fatal error while parsing JSON");
        if (res == ReadDictRes::kEndOfTrace ||
            res == ReadDictRes::kNeedsMoreData) {
          break;
        }

        if (res == ReadDictRes::kEndOfArray) {
          position_ = format_ == TraceFormat::kOuterDictionary
                          ? TracePosition::kDictionaryKey
                          : TracePosition::kEof;
          break;
        }

        base::Optional<int64_t> opt_ts =
            json_tracker->CoerceToTs((*value)["ts"]);
        int64_t ts = 0;
        if (opt_ts.has_value()) {
          ts = opt_ts.value();
        } else {
          // Metadata events may omit ts. In all other cases error:
          auto& ph = (*value)["ph"];
          if (!ph.isString() || *ph.asCString() != 'M') {
            context_->storage->IncrementStats(stats::json_tokenizer_failure);
            continue;
          }
        }
        trace_sorter->PushJsonValue(ts, std::move(value));
      }
      break;
    }
    case TracePosition::kEof: {
      break;
    }
  }
  *out = next;
  return util::OkStatus();
}
#endif

void JsonTraceTokenizer::NotifyEndOfFile() {}

}  // namespace trace_processor
}  // namespace perfetto

