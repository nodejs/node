// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_UNITTESTS_INTERPRETER_BYTECODE_EXPECTATIONS_PARSER_H_
#define TEST_UNITTESTS_INTERPRETER_BYTECODE_EXPECTATIONS_PARSER_H_

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace v8::internal::interpreter {

struct BytecodeExpectationsHeaderOptions {
  bool wrap = true;
  bool module = false;
  bool top_level = false;
  bool print_callee = false;
  std::string test_function_name;
  std::string extra_flags;
};

class BytecodeExpectationsParser {
 public:
  explicit BytecodeExpectationsParser(std::istream* is) : is_(is) {}

  BytecodeExpectationsHeaderOptions ParseHeader();
  bool ReadNextSnippet(std::string* string_out, int* line_out);
  std::string ReadToNextSeparator();

  int CurrentLine() const { return current_line_; }

 private:
  bool GetLine(std::string& line);

  std::istream* is_;
  int current_line_ = 0;
};

std::vector<std::filesystem::path> CollectGoldenFiles(
    const char* directory_path);

}  // namespace v8::internal::interpreter

#endif  // TEST_UNITTESTS_INTERPRETER_BYTECODE_EXPECTATIONS_PARSER_H_
