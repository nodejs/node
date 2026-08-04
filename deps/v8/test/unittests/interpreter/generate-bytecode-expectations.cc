// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8-array-buffer.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-local-handle.h"
#include "include/v8-message.h"
#include "src/base/logging.h"
#include "src/flags/save-flags.h"
#include "src/interpreter/interpreter.h"
#include "test/unittests/interpreter/bytecode-expectations-parser.h"
#include "test/unittests/interpreter/bytecode-expectations-printer.h"

#ifdef V8_OS_POSIX
#include <dirent.h>
#elif V8_OS_WIN
#include <windows.h>
#endif

using v8::internal::interpreter::BytecodeExpectationsHeaderOptions;
using v8::internal::interpreter::BytecodeExpectationsParser;
using v8::internal::interpreter::BytecodeExpectationsPrinter;
using v8::internal::interpreter::CollectGoldenFiles;

namespace {

const char* kGoldenFilesPath =
    "test/unittests/interpreter/bytecode_expectations/";

class ProgramOptions final {
 public:
  static ProgramOptions FromCommandLine(int argc, char** argv);

  ProgramOptions() = default;

  bool Validate() const;
  void UpdateFromHeader(BytecodeExpectationsParser* stream);
  void PrintHeader(std::ostream* stream) const;

  bool parsing_failed() const { return parsing_failed_; }
  bool print_help() const { return print_help_; }
  bool read_raw_js_snippet() const { return read_raw_js_snippet_; }
  bool read_from_stdin() const { return read_from_stdin_; }
  bool write_to_stdout() const {
    return output_filename_.empty() && !rebaseline_;
  }
  bool rebaseline() const { return rebaseline_; }
  bool check_baseline() const { return check_baseline_; }
  bool baseline() const { return rebaseline_ || check_baseline_; }
  bool wrap() const { return header_options_.wrap; }
  bool module() const { return header_options_.module; }
  bool top_level() const { return header_options_.top_level; }
  bool print_callee() const { return header_options_.print_callee; }
  bool verbose() const { return verbose_; }
  std::string extra_flags() const { return header_options_.extra_flags; }
  bool suppress_runtime_errors() const { return baseline() && !verbose_; }
  std::vector<std::filesystem::path> input_filenames() const {
    return input_filenames_;
  }
  std::string output_filename() const { return output_filename_; }
  std::string test_function_name() const {
    return header_options_.test_function_name;
  }
  const BytecodeExpectationsHeaderOptions& header_options() const {
    return header_options_;
  }

 private:
  bool parsing_failed_ = false;
  bool print_help_ = false;
  bool read_raw_js_snippet_ = false;
  bool read_from_stdin_ = false;
  bool rebaseline_ = false;
  bool check_baseline_ = false;
  bool verbose_ = false;
  BytecodeExpectationsHeaderOptions header_options_;
  std::vector<std::filesystem::path> input_filenames_;
  std::string output_filename_;
};

class V8_NODISCARD V8InitializationScope final {
 public:
  explicit V8InitializationScope(const char* exec_path);
  ~V8InitializationScope();
  V8InitializationScope(const V8InitializationScope&) = delete;
  V8InitializationScope& operator=(const V8InitializationScope&) = delete;

  v8::Platform* platform() const { return platform_.get(); }
  v8::Isolate* isolate() const { return isolate_; }

 private:
  std::unique_ptr<v8::Platform> platform_;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_;
  v8::Isolate* isolate_;
};

const char* BooleanToString(bool value) { return value ? "yes" : "no"; }

// static
ProgramOptions ProgramOptions::FromCommandLine(int argc, char** argv) {
  ProgramOptions options;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0) {
    } else if (strcmp(argv[i], "--raw-js") == 0) {
      options.read_raw_js_snippet_ = true;
    } else if (strcmp(argv[i], "--stdin") == 0) {
      options.read_from_stdin_ = true;
    } else if (strcmp(argv[i], "--rebaseline") == 0) {
      options.rebaseline_ = true;
    } else if (strcmp(argv[i], "--check-baseline") == 0) {
      options.check_baseline_ = true;
    } else if (strcmp(argv[i], "--no-wrap") == 0) {
      options.header_options_.wrap = false;
    } else if (strcmp(argv[i], "--module") == 0) {
      options.header_options_.module = true;
    } else if (strcmp(argv[i], "--top-level") == 0) {
      options.header_options_.top_level = true;
    } else if (strcmp(argv[i], "--print-callee") == 0) {
      options.header_options_.print_callee = true;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      options.verbose_ = true;
    } else if (strncmp(argv[i], "--output=", 9) == 0) {
      options.output_filename_ = argv[i] + 9;
    } else if (strncmp(argv[i], "--test-function-name=", 21) == 0) {
      options.header_options_.test_function_name = argv[i] + 21;
    } else if (strncmp(argv[i], "--", 2) != 0) {  // It doesn't start with --
      options.input_filenames_.push_back(argv[i]);
    } else {
      FATAL("Unknown option: %s", argv[i]);
    }
  }

  if (options.rebaseline() && options.check_baseline()) {
    FATAL("Can't check baseline and rebaseline at the same time.");
  }

  if ((options.check_baseline_ || options.rebaseline_) &&
      options.input_filenames_.empty()) {
    if (options.verbose_) {
      std::cout << "Looking for golden files in " << kGoldenFilesPath << '\n';
    }
    options.input_filenames_ = CollectGoldenFiles(kGoldenFilesPath);
  }

  return options;
}

bool ProgramOptions::Validate() const {
  if (parsing_failed_) return false;
  if (print_help_) return true;

  if (!read_from_stdin_ && input_filenames_.empty()) {
    FATAL("No input file specified.");
  }

  if (read_from_stdin_ && !input_filenames_.empty()) {
    FATAL("Reading from stdin, but input files supplied.");
  }

  if (baseline() && read_raw_js_snippet_) {
    FATAL("Cannot use --rebaseline or --check-baseline on a raw JS snippet.");
  }

  if (baseline() && !output_filename_.empty()) {
    FATAL(
        "Output file cannot be specified together with --rebaseline or "
        "--check-baseline.");
  }

  if (baseline() && read_from_stdin_) {
    FATAL("Cannot --rebaseline or --check-baseline when input is --stdin.");
  }

  if (input_filenames_.size() > 1 && !baseline() && !read_raw_js_snippet()) {
    FATAL(
        "Multiple input files, but no --rebaseline, --check-baseline or "
        "--raw-js specified.");
  }

  if (top_level() && !test_function_name().empty()) {
    FATAL("Test function name specified while processing top level code.");
  }

  if (module() && (!top_level() || wrap())) {
    FATAL("The flag --module currently requires --top-level and --no-wrap.");
  }

  return true;
}

void ProgramOptions::UpdateFromHeader(BytecodeExpectationsParser* parser) {
  header_options_ = parser->ParseHeader();
}

void ProgramOptions::PrintHeader(std::ostream* stream) const {
  *stream << "---"
          << "\nwrap: " << BooleanToString(header_options_.wrap);

  if (!header_options_.test_function_name.empty()) {
    *stream << "\ntest function name: " << header_options_.test_function_name;
  }

  if (header_options_.module) *stream << "\nmodule: yes";
  if (header_options_.top_level) *stream << "\ntop level: yes";
  if (header_options_.print_callee) *stream << "\nprint callee: yes";
  if (!header_options_.extra_flags.empty()) {
    *stream << "\nextra flags: " << header_options_.extra_flags;
  }

  *stream << "\n\n";
}

V8InitializationScope::V8InitializationScope(const char* exec_path)
    : platform_(v8::platform::NewDefaultPlatform()) {
  i::v8_flags.allow_natives_syntax = true;
  i::v8_flags.enable_lazy_source_positions = false;
  i::v8_flags.function_context_cells = false;

  // The bytecode expectations printer changes flags; this is not security
  // relevant, allow this.
  i::v8_flags.freeze_flags_after_init = false;

  v8::V8::InitializeICUDefaultLocation(exec_path);
  v8::V8::InitializeExternalStartupData(exec_path);
  v8::V8::InitializePlatform(platform_.get());
  v8::V8::Initialize();

  v8::Isolate::CreateParams create_params;
  allocator_.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  create_params.array_buffer_allocator = allocator_.get();

  isolate_ = v8::Isolate::New(create_params);
}

V8InitializationScope::~V8InitializationScope() {
  isolate_->Dispose();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
}

std::string ReadRawJSSnippet(std::istream* stream) {
  std::stringstream body_buffer;
  CHECK(body_buffer << stream->rdbuf());
  return body_buffer.str();
}

void ExtractSnippets(std::vector<std::string>* snippet_list,
                     BytecodeExpectationsParser* parser) {
  std::string snippet;
  int line;
  while (parser->ReadNextSnippet(&snippet, &line)) {
    snippet_list->push_back(snippet);
  }
}

void GenerateExpectationsFile(std::ostream* stream,
                              const std::vector<std::string>& snippet_list,
                              const V8InitializationScope& platform,
                              const ProgramOptions& options) {
  v8::internal::SaveFlags save_flags;
  v8::Isolate::Scope isolate_scope(platform.isolate());
  v8::HandleScope handle_scope(platform.isolate());
  v8::Local<v8::Context> context = v8::Context::New(platform.isolate());
  v8::Context::Scope context_scope(context);

  if (!options.extra_flags().empty()) {
    v8::V8::SetFlagsFromString(options.extra_flags().c_str(),
                               options.extra_flags().length());
  }

  BytecodeExpectationsPrinter printer(platform.isolate());
  printer.set_options(options.header_options());

  *stream << "#\n# Autogenerated by generate-bytecode-expectations.\n#\n\n";
  options.PrintHeader(stream);
  for (const std::string& snippet : snippet_list) {
    *stream << "---" << std::endl;
    printer.PrintExpectation(stream, snippet);
  }
}

bool WriteExpectationsFile(const std::vector<std::string>& snippet_list,
                           const V8InitializationScope& platform,
                           const ProgramOptions& options,
                           const std::filesystem::path& output_filename) {
  std::ofstream output_file_handle;
  if (!options.write_to_stdout()) {
    output_file_handle.open(output_filename, std::ios::binary);
    if (!output_file_handle.is_open()) {
      FATAL("Could not open %s for writing.", output_filename.c_str());
    }
  }
  std::ostream& output_stream =
      options.write_to_stdout() ? std::cout : output_file_handle;

  GenerateExpectationsFile(&output_stream, snippet_list, platform, options);

  return true;
}

std::string WriteExpectationsToString(
    const std::vector<std::string>& snippet_list,
    const V8InitializationScope& platform, const ProgramOptions& options) {
  std::stringstream output_string;

  GenerateExpectationsFile(&output_string, snippet_list, platform, options);

  return output_string.str();
}

void PrintMessage(v8::Local<v8::Message> message, v8::Local<v8::Value>) {
  std::cerr << "INFO: "
            << *v8::String::Utf8Value(v8::Isolate::GetCurrent(), message->Get())
            << '\n';
}

void DiscardMessage(v8::Local<v8::Message>, v8::Local<v8::Value>) {}

void PrintUsage(const char* exec_path) {
  std::cerr
      << "\nUsage: " << exec_path
      << " [OPTIONS]... [INPUT FILES]...\n\n"
         "Options:\n"
         "  --help        Print this help message.\n"
         "  --verbose     Emit messages about the progress of the tool.\n"
         "  --raw-js      Read raw JavaScript, instead of the output format.\n"
         "  --stdin       Read from standard input instead of file.\n"
         "  --rebaseline  Rebaseline input snippet file.\n"
         "  --check-baseline   Checks the current baseline is valid.\n"
         "  --no-wrap     Do not wrap the snippet in a function.\n"
         "  --print-callee     Print bytecode of callee, function should "
         "return arguments.callee.\n"
         "  --module      Compile as JavaScript module.\n"
         "  --test-function-name=foo  "
         "Specify the name of the test function.\n"
         "  --top-level   Process top level code, not the top-level function.\n"
         "  --output=file.name\n"
         "      Specify the output file. If not specified, output goes to "
         "stdout.\n"
         "  --pool-type=(number|string|mixed)\n"
         "      Specify the type of the entries in the constant pool "
         "(default: mixed).\n"
         "\n"
         "When using --rebaseline or --check-baseline, flags --no-wrap,\n"
         "--test-function-name and --pool-type will be overridden by the\n"
         "options specified in the input file header.\n\n"
         "Each raw JavaScript file is interpreted as a single snippet.\n\n"
         "This tool is intended as a help in writing tests.\n"
         "Please, DO NOT blindly copy and paste the output "
         "into the test suite.\n";
}

}  // namespace

bool CheckBaselineExpectations(const std::filesystem::path& input_filename,
                               const std::vector<std::string>& snippet_list,
                               const V8InitializationScope& platform,
                               const ProgramOptions& options) {
  std::string actual =
      WriteExpectationsToString(snippet_list, platform, options);

  std::ifstream input_stream(input_filename);
  if (!input_stream.is_open()) {
    FATAL("Could not open %s for reading.", input_filename.c_str());
  }

  bool check_failed = false;
  std::string expected((std::istreambuf_iterator<char>(input_stream)),
                       std::istreambuf_iterator<char>());
  if (expected != actual) {
    std::cerr << "Mismatch: " << input_filename;
    check_failed = true;
    if (expected.size() != actual.size()) {
      std::cerr << "  Expected size (" << expected.size()
                << ") != actual size (" << actual.size() << ")";
    }

    int line = 1;
    for (size_t i = 0; i < std::min(expected.size(), actual.size()); ++i) {
      if (expected[i] != actual[i]) {
        // Find the start of the line that has the mismatch carefully
        // handling the case where it's the first line that mismatches.
        size_t start = expected[i] != '\n' ? expected.rfind("\n", i)
                                           : actual.rfind("\n", i);
        if (start == std::string::npos) {
          start = 0;
        } else {
          ++start;
        }

        // If there is no new line, then these two lines will consume the
        // remaining characters in the string, because npos - start will
        // always be longer than the string itself.
        std::string expected_line =
            expected.substr(start, expected.find("\n", i) - start);
        std::string actual_line =
            actual.substr(start, actual.find("\n", i) - start);
        std::cerr << "  First mismatch on line " << line << ")";
        std::cerr << "    Expected : '" << expected_line << "'";
        std::cerr << "    Actual   : '" << actual_line << "'";
        break;
      }
      if (expected[i] == '\n') line++;
    }
  }
  return check_failed;
}

int main(int argc, char** argv) {
  ProgramOptions options = ProgramOptions::FromCommandLine(argc, argv);

  if (!options.Validate() || options.print_help()) {
    PrintUsage(argv[0]);
    return options.print_help() ? 0 : 1;
  }

  V8InitializationScope platform(argv[0]);
  {
    v8::Isolate::Scope isolate_scope(platform.isolate());
    platform.isolate()->AddMessageListener(
        options.suppress_runtime_errors() ? DiscardMessage : PrintMessage);
  }

  std::vector<std::string> snippet_list;

  if (options.read_from_stdin()) {
    // Rebaseline will never get here, so we will always take the
    // GenerateExpectationsFile at the end of this function.
    DCHECK(!options.rebaseline() && !options.check_baseline());
    if (options.read_raw_js_snippet()) {
      snippet_list.push_back(ReadRawJSSnippet(&std::cin));
    } else {
      BytecodeExpectationsParser parser(&std::cin);
      ExtractSnippets(&snippet_list, &parser);
    }
  } else {
    bool check_failed = false;
    for (const std::filesystem::path& input_filename :
         options.input_filenames()) {
      if (options.verbose()) {
        std::cerr << "Processing " << input_filename << '\n';
      }

      std::ifstream input_stream(input_filename);
      if (!input_stream.is_open()) {
        FATAL("Could not open %s for reading.", input_filename.c_str());
      }

      BytecodeExpectationsParser parser(&input_stream);

      ProgramOptions updated_options = options;
      if (options.baseline()) {
        updated_options.UpdateFromHeader(&parser);
        CHECK(updated_options.Validate());
      }

      if (options.read_raw_js_snippet()) {
        snippet_list.push_back(ReadRawJSSnippet(&input_stream));
      } else {
        ExtractSnippets(&snippet_list, &parser);
      }
      input_stream.close();

      if (options.rebaseline()) {
        if (!WriteExpectationsFile(snippet_list, platform, updated_options,
                                   input_filename)) {
          return 3;
        }
      } else if (options.check_baseline()) {
        check_failed |= CheckBaselineExpectations(input_filename, snippet_list,
                                                  platform, updated_options);
      }

      if (options.baseline()) {
        snippet_list.clear();
      }
    }
    if (check_failed) {
      return 4;
    }
  }

  if (!options.baseline()) {
    if (!WriteExpectationsFile(snippet_list, platform, options,
                               options.output_filename())) {
      return 3;
    }
  }
}
