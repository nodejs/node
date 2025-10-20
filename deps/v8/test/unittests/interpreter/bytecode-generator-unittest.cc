// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-generator.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "src/init/v8.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/interpreter/bytecode-expectations-parser.h"
#include "test/unittests/interpreter/bytecode-expectations-printer.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

namespace internal {
namespace interpreter {

class BytecodeGeneratorTest
    : public TestWithContext,
      public testing::WithParamInterface<std::filesystem::path> {
 public:
  BytecodeGeneratorTest() : printer_(isolate()) {}
  static void SetUpTestSuite() {
    i::v8_flags.allow_natives_syntax = true;
    i::v8_flags.enable_lazy_source_positions = false;
    i::v8_flags.lazy = false;
    i::v8_flags.flush_bytecode = false;
    i::v8_flags.function_context_cells = false;
    TestWithContext::SetUpTestSuite();
  }

  BytecodeExpectationsPrinter& printer() { return printer_; }

 private:
  BytecodeExpectationsPrinter printer_;
};

static const char* kGoldenFileDirectory =
    "test/unittests/interpreter/bytecode_expectations/";

struct GoldenCase {
  std::string snippet;
  std::string expectation;
  int line;
};

struct GoldenFile {
  BytecodeExpectationsHeaderOptions header;
  std::vector<GoldenCase> cases;
};

GoldenFile LoadGoldenFile(const std::filesystem::path& golden_path) {
  GoldenFile ret;
  std::ifstream file(golden_path);
  CHECK(file.is_open());

  BytecodeExpectationsParser parser(&file);
  ret.header = parser.ParseHeader();

  std::string snippet;
  int line;
  while (parser.ReadNextSnippet(&snippet, &line)) {
    std::string expected = parser.ReadToNextSeparator();
    ret.cases.emplace_back(snippet, expected, line);
  }

  return ret;
}

std::string BuildActual(const BytecodeExpectationsPrinter& printer,
                        const GoldenCase& golden_case) {
  std::ostringstream actual_stream;
  printer.PrintExpectation(&actual_stream, golden_case.snippet);
  return actual_stream.str();
}

std::string BuildExpected(const BytecodeExpectationsPrinter& printer,
                          const GoldenCase& golden_case) {
  std::ostringstream expected_stream;
  printer.PrintCodeSnippet(&expected_stream, golden_case.snippet);
  expected_stream << golden_case.expectation;
  return expected_stream.str();
}

// inplace left trim
static inline void ltrim(std::string* str) {
  str->erase(str->begin(),
             std::find_if(str->begin(), str->end(),
                          [](unsigned char ch) { return !std::isspace(ch); }));
}

// inplace right trim
static inline void rtrim(std::string* str) {
  str->erase(std::find_if(str->rbegin(), str->rend(),
                          [](unsigned char ch) { return !std::isspace(ch); })
                 .base(),
             str->end());
}

static inline std::string trim(std::string* str) {
  ltrim(str);
  rtrim(str);
  return *str;
}

void CompareTexts(const std::string& generated, const std::string& expected,
                  std::filesystem::path golden_file, int start_line) {
  std::istringstream generated_stream(generated);
  std::istringstream expected_stream(expected);
  std::string generated_line;
  std::string expected_line;
  int line_number = start_line;

  do {
    std::getline(generated_stream, generated_line);
    std::getline(expected_stream, expected_line);

    if (!generated_stream.good() && !expected_stream.good()) {
      return;
    }

    ASSERT_TRUE(generated_stream.good())
        << "Expected has extra lines after line " << line_number << "\n"
        << "  Expected: '" << expected_line << "'\n";
    ASSERT_TRUE(expected_stream.good())
        << "Generated has extra lines after line " << line_number << "\n"
        << "  Generated: '" << generated_line << "'\n";

    trim(&generated_line);
    trim(&expected_line);
    EXPECT_EQ(expected_line, generated_line)
        << "Inputs differ at " << golden_file << ":" << line_number << "\n";

    line_number++;
  } while (true);
}

TEST(BytecodeGeneratorInitTest, HasGoldenFiles) {
  std::vector<std::filesystem::path> golden_files =
      CollectGoldenFiles(kGoldenFileDirectory);
  CHECK(!golden_files.empty());
}

TEST_P(BytecodeGeneratorTest, ExpectationNonEmpty) {
  GoldenFile golden = LoadGoldenFile(GetParam());
  for (const GoldenCase& golden_case : golden.cases) {
    std::string expectation = golden_case.expectation;
    trim(&expectation);
    CHECK(!expectation.empty());
  }
}

TEST_P(BytecodeGeneratorTest, CompilationMatchesExpectation) {
  GoldenFile golden = LoadGoldenFile(GetParam());
  printer().set_options(golden.header);
  for (const GoldenCase& golden_case : golden.cases) {
    CompareTexts(BuildActual(printer(), golden_case),
                 BuildExpected(printer(), golden_case), GetParam(),
                 golden_case.line);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllGolden, BytecodeGeneratorTest,
    // Run the BytecodeGeneratorTest for each golden file
    // in the golden file directory.
    testing::ValuesIn(CollectGoldenFiles(kGoldenFileDirectory)),
    // Print the golden file's filename, without the
    // extension, as the test value.
    [](const testing::TestParamInfo<std::filesystem::path>& info) {
      std::filesystem::path file = info.param;
      // A CHECK_EQ here breaks the macro expansion somehow.
      CHECK(".golden" == file.extension().string());
      return file.stem().string();
    });

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
