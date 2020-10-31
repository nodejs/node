// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/util/internal/field_mask_utility.h>

#include <google/protobuf/util/internal/utility.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/status_macros.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

namespace {

// Appends a FieldMask path segment to a prefix.
std::string AppendPathSegmentToPrefix(StringPiece prefix,
                                      StringPiece segment) {
  if (prefix.empty()) {
    return std::string(segment);
  }
  if (segment.empty()) {
    return std::string(prefix);
  }
  // If the segment is a map key, appends it to the prefix without the ".".
  if (HasPrefixString(segment, "[\"")) {
    return StrCat(prefix, segment);
  }
  return StrCat(prefix, ".", segment);
}

}  // namespace

std::string ConvertFieldMaskPath(const StringPiece path,
                                 ConverterCallback converter) {
  std::string result;
  result.reserve(path.size() << 1);

  bool is_quoted = false;
  bool is_escaping = false;
  int current_segment_start = 0;

  // Loops until 1 passed the end of the input to make handling the last
  // segment easier.
  for (size_t i = 0; i <= path.size(); ++i) {
    // Outputs quoted string as-is.
    if (is_quoted) {
      if (i == path.size()) {
        break;
      }
      result.push_back(path[i]);
      if (is_escaping) {
        is_escaping = false;
      } else if (path[i] == '\\') {
        is_escaping = true;
      } else if (path[i] == '\"') {
        current_segment_start = i + 1;
        is_quoted = false;
      }
      continue;
    }
    if (i == path.size() || path[i] == '.' || path[i] == '(' ||
        path[i] == ')' || path[i] == '\"') {
      result += converter(
          path.substr(current_segment_start, i - current_segment_start));
      if (i < path.size()) {
        result.push_back(path[i]);
      }
      current_segment_start = i + 1;
    }
    if (i < path.size() && path[i] == '\"') {
      is_quoted = true;
    }
  }
  return result;
}

util::Status DecodeCompactFieldMaskPaths(StringPiece paths,
                                           PathSinkCallback path_sink) {
  std::stack<std::string> prefix;
  int length = paths.length();
  int previous_position = 0;
  bool in_map_key = false;
  bool is_escaping = false;
  // Loops until 1 passed the end of the input to make the handle of the last
  // segment easier.
  for (int i = 0; i <= length; ++i) {
    if (i != length) {
      // Skips everything in a map key until we hit the end of it, which is
      // marked by an un-escaped '"' immediately followed by a ']'.
      if (in_map_key) {
        if (is_escaping) {
          is_escaping = false;
          continue;
        }
        if (paths[i] == '\\') {
          is_escaping = true;
          continue;
        }
        if (paths[i] != '\"') {
          continue;
        }
        // Un-escaped '"' must be followed with a ']'.
        if (i >= length - 1 || paths[i + 1] != ']') {
          return util::Status(
              util::error::INVALID_ARGUMENT,
              StrCat(
                  "Invalid FieldMask '", paths,
                  "'. Map keys should be represented as [\"some_key\"]."));
        }
        // The end of the map key ("\"]") has been found.
        in_map_key = false;
        // Skips ']'.
        i++;
        // Checks whether the key ends at the end of a path segment.
        if (i < length - 1 && paths[i + 1] != '.' && paths[i + 1] != ',' &&
            paths[i + 1] != ')' && paths[i + 1] != '(') {
          return util::Status(
              util::error::INVALID_ARGUMENT,
              StrCat(
                  "Invalid FieldMask '", paths,
                  "'. Map keys should be at the end of a path segment."));
        }
        is_escaping = false;
        continue;
      }

      // We are not in a map key, look for the start of one.
      if (paths[i] == '[') {
        if (i >= length - 1 || paths[i + 1] != '\"') {
          return util::Status(
              util::error::INVALID_ARGUMENT,
              StrCat(
                  "Invalid FieldMask '", paths,
                  "'. Map keys should be represented as [\"some_key\"]."));
        }
        // "[\"" starts a map key.
        in_map_key = true;
        i++;  // Skips the '\"'.
        continue;
      }
      // If the current character is not a special character (',', '(' or ')'),
      // continue to the next.
      if (paths[i] != ',' && paths[i] != ')' && paths[i] != '(') {
        continue;
      }
    }
    // Gets the current segment - sub-string between previous position (after
    // '(', ')', ',', or the beginning of the input) and the current position.
    StringPiece segment =
        paths.substr(previous_position, i - previous_position);
    std::string current_prefix = prefix.empty() ? "" : prefix.top();

    if (i < length && paths[i] == '(') {
      // Builds a prefix and save it into the stack.
      prefix.push(AppendPathSegmentToPrefix(current_prefix, segment));
    } else if (!segment.empty()) {
      // When the current charactor is ')', ',' or the current position has
      // passed the end of the input, builds and outputs a new paths by
      // concatenating the last prefix with the current segment.
      RETURN_IF_ERROR(
          path_sink(AppendPathSegmentToPrefix(current_prefix, segment)));
    }

    // Removes the last prefix after seeing a ')'.
    if (i < length && paths[i] == ')') {
      if (prefix.empty()) {
        return util::Status(
            util::error::INVALID_ARGUMENT,
            StrCat("Invalid FieldMask '", paths,
                         "'. Cannot find matching '(' for all ')'."));
      }
      prefix.pop();
    }
    previous_position = i + 1;
  }
  if (in_map_key) {
    return util::Status(
        util::error::INVALID_ARGUMENT,
        StrCat("Invalid FieldMask '", paths,
                     "'. Cannot find matching ']' for all '['."));
  }
  if (!prefix.empty()) {
    return util::Status(
        util::error::INVALID_ARGUMENT,
        StrCat("Invalid FieldMask '", paths,
                     "'. Cannot find matching ')' for all '('."));
  }
  return util::Status();
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
