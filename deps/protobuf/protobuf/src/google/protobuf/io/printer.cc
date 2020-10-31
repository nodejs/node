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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <cctype>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

namespace google {
namespace protobuf {
namespace io {

Printer::Printer(ZeroCopyOutputStream* output, char variable_delimiter)
    : variable_delimiter_(variable_delimiter),
      output_(output),
      buffer_(NULL),
      buffer_size_(0),
      offset_(0),
      at_start_of_line_(true),
      failed_(false),
      annotation_collector_(NULL) {}

Printer::Printer(ZeroCopyOutputStream* output, char variable_delimiter,
                 AnnotationCollector* annotation_collector)
    : variable_delimiter_(variable_delimiter),
      output_(output),
      buffer_(NULL),
      buffer_size_(0),
      offset_(0),
      at_start_of_line_(true),
      failed_(false),
      annotation_collector_(annotation_collector) {}

Printer::~Printer() {
  // Only BackUp() if we have called Next() at least once and never failed.
  if (buffer_size_ > 0 && !failed_) {
    output_->BackUp(buffer_size_);
  }
}

bool Printer::GetSubstitutionRange(const char* varname,
                                   std::pair<size_t, size_t>* range) {
  std::map<std::string, std::pair<size_t, size_t> >::const_iterator iter =
      substitutions_.find(varname);
  if (iter == substitutions_.end()) {
    GOOGLE_LOG(DFATAL) << " Undefined variable in annotation: " << varname;
    return false;
  }
  if (iter->second.first > iter->second.second) {
    GOOGLE_LOG(DFATAL) << " Variable used for annotation used multiple times: "
                << varname;
    return false;
  }
  *range = iter->second;
  return true;
}

void Printer::Annotate(const char* begin_varname, const char* end_varname,
                       const std::string& file_path,
                       const std::vector<int>& path) {
  if (annotation_collector_ == NULL) {
    // Can't generate signatures with this Printer.
    return;
  }
  std::pair<size_t, size_t> begin, end;
  if (!GetSubstitutionRange(begin_varname, &begin) ||
      !GetSubstitutionRange(end_varname, &end)) {
    return;
  }
  if (begin.first > end.second) {
    GOOGLE_LOG(DFATAL) << "  Annotation has negative length from " << begin_varname
                << " to " << end_varname;
  } else {
    annotation_collector_->AddAnnotation(begin.first, end.second, file_path,
                                         path);
  }
}

void Printer::Print(const std::map<std::string, std::string>& variables,
                    const char* text) {
  int size = strlen(text);
  int pos = 0;  // The number of bytes we've written so far.
  substitutions_.clear();
  line_start_variables_.clear();

  for (int i = 0; i < size; i++) {
    if (text[i] == '\n') {
      // Saw newline.  If there is more text, we may need to insert an indent
      // here.  So, write what we have so far, including the '\n'.
      WriteRaw(text + pos, i - pos + 1);
      pos = i + 1;

      // Setting this true will cause the next WriteRaw() to insert an indent
      // first.
      at_start_of_line_ = true;
      line_start_variables_.clear();

    } else if (text[i] == variable_delimiter_) {
      // Saw the start of a variable name.

      // Write what we have so far.
      WriteRaw(text + pos, i - pos);
      pos = i + 1;

      // Find closing delimiter.
      const char* end = strchr(text + pos, variable_delimiter_);
      if (end == NULL) {
        GOOGLE_LOG(DFATAL) << " Unclosed variable name.";
        end = text + pos;
      }
      int endpos = end - text;

      std::string varname(text + pos, endpos - pos);
      if (varname.empty()) {
        // Two delimiters in a row reduce to a literal delimiter character.
        WriteRaw(&variable_delimiter_, 1);
      } else {
        // Replace with the variable's value.
        std::map<std::string, std::string>::const_iterator iter =
            variables.find(varname);
        if (iter == variables.end()) {
          GOOGLE_LOG(DFATAL) << " Undefined variable: " << varname;
        } else {
          if (at_start_of_line_ && iter->second.empty()) {
            line_start_variables_.push_back(varname);
          }
          WriteRaw(iter->second.data(), iter->second.size());
          std::pair<std::map<std::string, std::pair<size_t, size_t> >::iterator,
                    bool>
              inserted = substitutions_.insert(std::make_pair(
                  varname,
                  std::make_pair(offset_ - iter->second.size(), offset_)));
          if (!inserted.second) {
            // This variable was used multiple times.  Make its span have
            // negative length so we can detect it if it gets used in an
            // annotation.
            inserted.first->second = std::make_pair(1, 0);
          }
        }
      }

      // Advance past this variable.
      i = endpos;
      pos = endpos + 1;
    }
  }

  // Write the rest.
  WriteRaw(text + pos, size - pos);
}

void Printer::Indent() { indent_ += "  "; }

void Printer::Outdent() {
  if (indent_.empty()) {
    GOOGLE_LOG(DFATAL) << " Outdent() without matching Indent().";
    return;
  }

  indent_.resize(indent_.size() - 2);
}

void Printer::PrintRaw(const std::string& data) {
  WriteRaw(data.data(), data.size());
}

void Printer::PrintRaw(const char* data) {
  if (failed_) return;
  WriteRaw(data, strlen(data));
}

void Printer::WriteRaw(const char* data, int size) {
  if (failed_) return;
  if (size == 0) return;

  if (at_start_of_line_ && (size > 0) && (data[0] != '\n')) {
    // Insert an indent.
    at_start_of_line_ = false;
    CopyToBuffer(indent_.data(), indent_.size());
    if (failed_) return;
    // Fix up empty variables (e.g., "{") that should be annotated as
    // coming after the indent.
    for (std::vector<std::string>::iterator i = line_start_variables_.begin();
         i != line_start_variables_.end(); ++i) {
      substitutions_[*i].first += indent_.size();
      substitutions_[*i].second += indent_.size();
    }
  }

  // If we're going to write any data, clear line_start_variables_, since
  // we've either updated them in the block above or they no longer refer to
  // the current line.
  line_start_variables_.clear();

  CopyToBuffer(data, size);
}

bool Printer::Next() {
  do {
    void* void_buffer;
    if (!output_->Next(&void_buffer, &buffer_size_)) {
      failed_ = true;
      return false;
    }
    buffer_ = reinterpret_cast<char*>(void_buffer);
  } while (buffer_size_ == 0);
  return true;
}

void Printer::CopyToBuffer(const char* data, int size) {
  if (failed_) return;
  if (size == 0) return;

  while (size > buffer_size_) {
    // Data exceeds space in the buffer.  Copy what we can and request a
    // new buffer.
    if (buffer_size_ > 0) {
      memcpy(buffer_, data, buffer_size_);
      offset_ += buffer_size_;
      data += buffer_size_;
      size -= buffer_size_;
    }
    void* void_buffer;
    failed_ = !output_->Next(&void_buffer, &buffer_size_);
    if (failed_) return;
    buffer_ = reinterpret_cast<char*>(void_buffer);
  }

  // Buffer is big enough to receive the data; copy it.
  memcpy(buffer_, data, size);
  buffer_ += size;
  buffer_size_ -= size;
  offset_ += size;
}

void Printer::IndentIfAtStart() {
  if (at_start_of_line_) {
    CopyToBuffer(indent_.data(), indent_.size());
    at_start_of_line_ = false;
  }
}

void Printer::FormatInternal(const std::vector<std::string>& args,
                             const std::map<std::string, std::string>& vars,
                             const char* format) {
  auto save = format;
  int arg_index = 0;
  std::vector<AnnotationCollector::Annotation> annotations;
  while (*format) {
    char c = *format++;
    switch (c) {
      case '$':
        format = WriteVariable(args, vars, format, &arg_index, &annotations);
        continue;
      case '\n':
        at_start_of_line_ = true;
        line_start_variables_.clear();
        break;
      default:
        IndentIfAtStart();
        break;
    }
    push_back(c);
  }
  if (arg_index != args.size()) {
    GOOGLE_LOG(FATAL) << " Unused arguments. " << save;
  }
  if (!annotations.empty()) {
    GOOGLE_LOG(FATAL) << " Annotation range is not-closed, expect $}$. " << save;
  }
}

const char* Printer::WriteVariable(
    const std::vector<string>& args,
    const std::map<std::string, std::string>& vars, const char* format,
    int* arg_index, std::vector<AnnotationCollector::Annotation>* annotations) {
  auto start = format;
  auto end = strchr(format, '$');
  if (!end) {
    GOOGLE_LOG(FATAL) << " Unclosed variable name.";
  }
  format = end + 1;
  if (end == start) {
    // "$$" is an escape for just '$'
    IndentIfAtStart();
    push_back('$');
    return format;
  }
  if (*start == '{') {
    GOOGLE_CHECK(std::isdigit(start[1]));
    GOOGLE_CHECK_EQ(end - start, 2);
    int idx = start[1] - '1';
    if (idx < 0 || idx >= args.size()) {
      GOOGLE_LOG(FATAL) << "Annotation ${" << idx + 1 << "$ is out of bounds.";
    }
    if (idx > *arg_index) {
      GOOGLE_LOG(FATAL) << "Annotation arg must be in correct order as given. Expected"
                 << " ${" << (*arg_index) + 1 << "$ got ${" << idx + 1 << "$.";
    } else if (idx == *arg_index) {
      (*arg_index)++;
    }
    IndentIfAtStart();
    annotations->push_back({{offset_, 0}, args[idx]});
    return format;
  } else if (*start == '}') {
    GOOGLE_CHECK(annotations);
    if (annotations->empty()) {
      GOOGLE_LOG(FATAL) << "Unexpected end of annotation found.";
    }
    auto& a = annotations->back();
    a.first.second = offset_;
    if (annotation_collector_) annotation_collector_->AddAnnotationNew(a);
    annotations->pop_back();
    return format;
  }
  auto start_var = start;
  while (start_var < end && *start_var == ' ') start_var++;
  if (start_var == end) {
    GOOGLE_LOG(FATAL) << " Empty variable.";
  }
  auto end_var = end;
  while (start_var < end_var && *(end_var - 1) == ' ') end_var--;
  std::string var_name{
      start_var, static_cast<std::string::size_type>(end_var - start_var)};
  std::string sub;
  if (std::isdigit(var_name[0])) {
    GOOGLE_CHECK_EQ(var_name.size(), 1);  // No need for multi-digits
    int idx = var_name[0] - '1';   // Start counting at 1
    GOOGLE_CHECK_GE(idx, 0);
    if (idx >= args.size()) {
      GOOGLE_LOG(FATAL) << "Argument $" << idx + 1 << "$ is out of bounds.";
    }
    if (idx > *arg_index) {
      GOOGLE_LOG(FATAL) << "Arguments must be used in same order as given. Expected $"
                 << (*arg_index) + 1 << "$ got $" << idx + 1 << "$.";
    } else if (idx == *arg_index) {
      (*arg_index)++;
    }
    sub = args[idx];
  } else {
    auto it = vars.find(var_name);
    if (it == vars.end()) {
      GOOGLE_LOG(FATAL) << " Unknown variable: " << var_name << ".";
    }
    sub = it->second;
  }

  // By returning here in case of empty we also skip possible spaces inside
  // the $...$, i.e. "void$ dllexpor$ f();" -> "void f();" in the empty case.
  if (sub.empty()) return format;

  // We're going to write something non-empty so we need a possible indent.
  IndentIfAtStart();

  // Write the possible spaces in front.
  CopyToBuffer(start, start_var - start);
  // Write a non-empty substituted variable.
  CopyToBuffer(sub.c_str(), sub.size());
  // Finish off with writing possible trailing spaces.
  CopyToBuffer(end_var, end - end_var);
  return format;
}

}  // namespace io
}  // namespace protobuf
}  // namespace google
