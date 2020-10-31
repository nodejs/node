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

#ifndef SRC_TRACED_PROBES_FTRACE_FORMAT_PARSER_H_
#define SRC_TRACED_PROBES_FTRACE_FORMAT_PARSER_H_

#include <stdint.h>
#include <string>

#include <iosfwd>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace perfetto {

struct FtraceEvent {
  struct Field {
    std::string type_and_name;
    uint16_t offset;
    uint16_t size;
    bool is_signed;

    bool operator==(const Field& other) const {
      return std::tie(type_and_name, offset, size, is_signed) ==
             std::tie(other.type_and_name, other.offset, other.size,
                      other.is_signed);
    }
  };

  // When making changes / additions to these structs, remember that
  // proto_translation_table.cc has some fallback code for 'page_header' and
  // the 'print' event that fill this struct.
  std::string name;
  uint32_t id;
  std::vector<Field> common_fields;
  std::vector<Field> fields;
};

std::string GetNameFromTypeAndName(const std::string& type_and_name);

// Allow gtest to pretty print FtraceEvent::Field.
::std::ostream& operator<<(::std::ostream& os, const FtraceEvent::Field&);
void PrintTo(const FtraceEvent::Field& args, ::std::ostream* os);

// Parses only the body (i.e. contents of format) of an ftrace event format
// file, e.g.
//
//   field:unsigned short common_type;  offset:0;  size:2;  signed:0;
//   field:unsigned char common_flags;  offset:2;  size:1;  signed:0;
//   field:unsigned char common_preempt_count;  offset:3;  size:1;  signed:0;
//   field:int common_pid;  offset:4;  size:4;  signed:1;
//
//   field:dev_t dev;  offset:8;  size:4;  signed:0;
//   field:ino_t ino;  offset:12;  size:4;  signed:0;
//   field:ino_t dir;  offset:16;  size:4;  signed:0;
//   field:__u16 mode;  offset:20;  size:2;  signed:0;
bool ParseFtraceEventBody(std::string input,
                          std::vector<FtraceEvent::Field>* common_fields,
                          std::vector<FtraceEvent::Field>* fields,
                          bool disable_logging_for_testing = false);
// Parses ftrace event format file. This includes the headers specifying
// name and ID of the event, e.g.
//
// name: ext4_allocate_inode
// ID: 309
// format:
//   field:unsigned short common_type;  offset:0;  size:2;  signed:0;
//   field:unsigned char common_flags;  offset:2;  size:1;  signed:0;
//   field:unsigned char common_preempt_count;  offset:3;  size:1;  signed:0;
//   field:int common_pid;  offset:4;  size:4;  signed:1;
//
//   field:dev_t dev;  offset:8;  size:4;  signed:0;
//   field:ino_t ino;  offset:12;  size:4;  signed:0;
//   field:ino_t dir;  offset:16;  size:4;  signed:0;
//   field:__u16 mode;  offset:20;  size:2;  signed:0;
bool ParseFtraceEvent(std::string input, FtraceEvent* output = nullptr);

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_FORMAT_PARSER_H_
