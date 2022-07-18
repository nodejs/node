// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8-initialization.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/module-decoder-impl.h"
#include "src/wasm/names-provider.h"
#include "src/wasm/string-builder-multiline.h"
#include "src/wasm/wasm-disassembler-impl.h"
#include "src/wasm/wasm-opcodes-inl.h"

int PrintHelp(char** argv) {
  std::cerr << "Usage: Specify an action and a module name in any order.\n"
            << "The action can be any of:\n"

            << " --help\n"
            << "     Print this help and exit.\n"

            << " --list-functions\n"
            << "     List functions in the given module\n"

            << " --section-stats\n"
            << "     Show information about sections in the given module\n"

            << " --single-wat FUNC_INDEX\n"
            << "     Dump function FUNC_INDEX in .wat format\n"

            << " --full-wat\n"
            << "     Dump full module in .wat format\n"

            << " --single-hexdump FUNC_INDEX\n"
            << "     Dump function FUNC_INDEX in annotated hex format\n"

            << "The module name must be a file name.\n";
  return 1;
}

namespace v8 {
namespace internal {
namespace wasm {

enum class OutputMode { kWat, kHexDump };
static constexpr char kHexChars[] = "0123456789abcdef";

char* PrintHexBytesCore(char* ptr, uint32_t num_bytes, const byte* start) {
  for (uint32_t i = 0; i < num_bytes; i++) {
    byte b = *(start + i);
    *(ptr++) = '0';
    *(ptr++) = 'x';
    *(ptr++) = kHexChars[b >> 4];
    *(ptr++) = kHexChars[b & 0xF];
    *(ptr++) = ',';
    *(ptr++) = ' ';
  }
  return ptr;
}

// A variant of FunctionBodyDisassembler that can produce "annotated hex dump"
// format, e.g.:
//     0xfb, 0x07, 0x01,  // struct.new $type1
class ExtendedFunctionDis : public FunctionBodyDisassembler {
 public:
  ExtendedFunctionDis(Zone* zone, const WasmModule* module, uint32_t func_index,
                      WasmFeatures* detected, const FunctionSig* sig,
                      const byte* start, const byte* end, uint32_t offset,
                      NamesProvider* names)
      : FunctionBodyDisassembler(zone, module, func_index, detected, sig, start,
                                 end, offset, names) {}

  static constexpr uint32_t kWeDontCareAboutByteCodeOffsetsHere = 0;

  void HexDump(MultiLineStringBuilder& out, FunctionHeader include_header) {
    out_ = &out;
    if (!more()) return;  // Fuzzers...
    // Print header.
    if (include_header == kPrintHeader) {
      out << "  // func ";
      names_->PrintFunctionName(out, func_index_, NamesProvider::kDevTools);
      PrintSignatureOneLine(out, sig_, func_index_, names_, true,
                            NamesProvider::kIndexAsComment);
      out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
    }

    // Decode and print locals.
    uint32_t locals_length;
    InitializeLocalsFromSig();
    DecodeLocals(pc_, &locals_length, 0);
    if (failed()) {
      // TODO(jkummerow): Better error handling.
      out << "Failed to decode locals";
      return;
    }
    uint32_t total_length = 0;
    uint32_t length;
    uint32_t entries = read_u32v<validate>(pc_, &length);
    PrintHexBytes(out, length, pc_, 4);
    out << " // " << entries << " entries in locals list";
    out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
    total_length += length;
    while (entries-- > 0) {
      uint32_t count_length;
      uint32_t count = read_u32v<validate>(pc_ + total_length, &count_length);
      uint32_t type_length;
      ValueType type = value_type_reader::read_value_type<validate>(
          this, pc_ + total_length + count_length, &type_length, nullptr,
          WasmFeatures::All());
      PrintHexBytes(out, count_length + type_length, pc_ + total_length, 4);
      out << " // " << count << (count != 1 ? " locals" : " local")
          << " of type ";
      names_->PrintValueType(out, type);
      out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
      total_length += count_length + type_length;
    }

    consume_bytes(locals_length);

    // Main loop.
    while (pc_ < end_) {
      WasmOpcode opcode = GetOpcode();
      current_opcode_ = opcode;  // Some immediates need to know this.
      StringBuilder immediates;
      uint32_t length = PrintImmediatesAndGetLength(immediates);
      PrintHexBytes(out, length, pc_, 4);
      if (opcode == kExprEnd) {
        out << " // end";
        if (label_stack_.size() > 0) {
          const LabelInfo& label = label_stack_.back();
          if (label.start != nullptr) {
            out << " ";
            out.write(label.start, label.length);
          }
          label_stack_.pop_back();
        }
      } else {
        out << " // " << WasmOpcodes::OpcodeName(opcode);
      }
      out.write(immediates.start(), immediates.length());
      if (opcode == kExprBlock || opcode == kExprIf || opcode == kExprLoop ||
          opcode == kExprTry) {
        label_stack_.emplace_back(out.line_number(), out.length(),
                                  label_occurrence_index_++);
      }
      out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
      pc_ += length;
    }

    if (pc_ != end_) {
      // TODO(jkummerow): Better error handling.
      out << "Beyond end of code\n";
    }
  }

  void HexdumpConstantExpression(MultiLineStringBuilder& out) {
    while (pc_ < end_) {
      WasmOpcode opcode = GetOpcode();
      current_opcode_ = opcode;  // Some immediates need to know this.
      StringBuilder immediates;
      uint32_t length = PrintImmediatesAndGetLength(immediates);
      // Don't print the final "end" separately.
      if (pc_ + length + 1 == end_ && *(pc_ + length) == kExprEnd) {
        length++;
      }
      PrintHexBytes(out, length, pc_, 4);
      out << " // " << WasmOpcodes::OpcodeName(opcode);
      out.write(immediates.start(), immediates.length());
      out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
      pc_ += length;
    }
  }

  void PrintHexBytes(StringBuilder& out, uint32_t num_bytes, const byte* start,
                     uint32_t fill_to_minimum = 0) {
    constexpr int kCharsPerByte = 6;  // Length of "0xFF, ".
    uint32_t max = std::max(num_bytes, fill_to_minimum) * kCharsPerByte + 2;
    char* ptr = out.allocate(max);
    *(ptr++) = ' ';
    *(ptr++) = ' ';
    ptr = PrintHexBytesCore(ptr, num_bytes, start);
    if (fill_to_minimum > num_bytes) {
      memset(ptr, ' ', (fill_to_minimum - num_bytes) * kCharsPerByte);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////

class FormatConverter {
 public:
  explicit FormatConverter(std::string path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
      std::cerr << "Failed to open " << path << "!\n";
      return;
    }
    raw_bytes_ = std::vector<byte>(std::istreambuf_iterator<char>(input), {});
    if (raw_bytes_.size() < 8 || raw_bytes_[0] != 0 || raw_bytes_[1] != 'a' ||
        raw_bytes_[2] != 's' || raw_bytes_[3] != 'm') {
      std::cerr << "That's not a Wasm module!\n";
      return;
    }
    base::Vector<const byte> wire_bytes(raw_bytes_.data(), raw_bytes_.size());
    wire_bytes_ = ModuleWireBytes({raw_bytes_.data(), raw_bytes_.size()});
    ModuleResult result =
        DecodeWasmModuleForDisassembler(start(), end(), &allocator_);
    if (result.failed()) {
      WasmError error = result.error();
      std::cerr << "Decoding error: " << error.message() << " at offset "
                << error.offset() << "\n";
      // TODO(jkummerow): Show some disassembly.
      return;
    }
    ok_ = true;
    module_ = result.value();
    names_provider_ =
        std::make_unique<NamesProvider>(module_.get(), wire_bytes);
  }

  bool ok() const { return ok_; }

  void ListFunctions() {
    DCHECK(ok_);
    const WasmModule* m = module();
    uint32_t num_functions = static_cast<uint32_t>(m->functions.size());
    std::cout << "There are " << num_functions << " functions ("
              << m->num_imported_functions << " imported, "
              << m->num_declared_functions
              << " locally defined); the following have names:\n";
    for (uint32_t i = 0; i < num_functions; i++) {
      StringBuilder sb;
      names()->PrintFunctionName(sb, i);
      if (sb.length() == 0) continue;
      std::string name(sb.start(), sb.length());
      std::cout << i << " " << name << "\n";
    }
  }

  void SectionStats() {
    DCHECK(ok_);
    Decoder decoder(start(), end());
    static constexpr int kModuleHeaderSize = 8;
    decoder.consume_bytes(kModuleHeaderSize, "module header");

    uint32_t module_size = static_cast<uint32_t>(end() - start());
    int digits = 2;
    for (uint32_t comparator = 100; module_size >= comparator;
         comparator *= 10) {
      digits++;
    }
    size_t kMinNameLength = 8;
    // 18 = kMinNameLength + strlen(" section: ").
    std::cout << std::setw(18) << std::left << "Module size: ";
    std::cout << std::setw(digits) << std::right << module_size << " bytes\n";
    for (WasmSectionIterator it(&decoder); it.more(); it.advance(true)) {
      const char* name = SectionName(it.section_code());
      size_t name_len = strlen(name);
      std::cout << SectionName(it.section_code()) << " section: ";
      for (; name_len < kMinNameLength; name_len++) std::cout << " ";

      uint32_t length = it.section_length();
      std::cout << std::setw(name_len > kMinNameLength ? 0 : digits) << length
                << " bytes / ";

      std::cout << std::fixed << std::setprecision(1) << std::setw(4)
                << 100.0 * length / module_size;
      std::cout << "% of total\n";
    }
  }

  void DisassembleFunction(uint32_t func_index, MultiLineStringBuilder& out,
                           OutputMode mode) {
    DCHECK(ok_);
    if (func_index >= module()->functions.size()) {
      out << "Invalid function index!\n";
      return;
    }
    if (func_index < module()->num_imported_functions) {
      out << "Can't disassemble imported functions!\n";
      return;
    }
    const WasmFunction* func = &module()->functions[func_index];
    Zone zone(&allocator_, "disassembler");
    WasmFeatures detected;
    base::Vector<const byte> code = wire_bytes_.GetFunctionBytes(func);

    ExtendedFunctionDis d(&zone, module(), func_index, &detected, func->sig,
                          code.begin(), code.end(), func->code.offset(),
                          names());
    if (mode == OutputMode::kWat) {
      d.DecodeAsWat(out, {0, 1});
    } else if (mode == OutputMode::kHexDump) {
      d.HexDump(out, FunctionBodyDisassembler::kPrintHeader);
    }

    // Print any types that were used by the function.
    out.NextLine(0);
    ModuleDisassembler md(out, module(), names(), wire_bytes_,
                          ModuleDisassembler::kSkipByteOffsets, &allocator_);
    for (uint32_t type_index : d.used_types()) {
      md.PrintTypeDefinition(type_index, {0, 1},
                             NamesProvider::kIndexAsComment);
    }
  }

  void WatForModule(MultiLineStringBuilder& out) {
    DCHECK(ok_);
    ModuleDisassembler md(out, module(), names(), wire_bytes_,
                          ModuleDisassembler::kSkipByteOffsets, &allocator_);
    md.PrintModule({0, 2});
  }

 private:
  byte* start() { return raw_bytes_.data(); }
  byte* end() { return start() + raw_bytes_.size(); }
  const WasmModule* module() { return module_.get(); }
  NamesProvider* names() { return names_provider_.get(); }

  AccountingAllocator allocator_;
  bool ok_{false};
  std::vector<byte> raw_bytes_;
  ModuleWireBytes wire_bytes_{{}};
  std::shared_ptr<WasmModule> module_;
  std::unique_ptr<NamesProvider> names_provider_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

using FormatConverter = v8::internal::wasm::FormatConverter;
using OutputMode = v8::internal::wasm::OutputMode;
using MultiLineStringBuilder = v8::internal::wasm::MultiLineStringBuilder;

enum class Action {
  kUnset,
  kHelp,
  kListFunctions,
  kSectionStats,
  kFullWat,
  kSingleWat,
  kSingleHexdump,
};

struct Options {
  const char* filename = nullptr;
  Action action = Action::kUnset;
  int func_index = -1;
};

void ListFunctions(const Options& options) {
  FormatConverter fc(options.filename);
  if (fc.ok()) fc.ListFunctions();
}

void SectionStats(const Options& options) {
  FormatConverter fc(options.filename);
  if (fc.ok()) fc.SectionStats();
}

void WatForFunction(const Options& options) {
  FormatConverter fc(options.filename);
  if (!fc.ok()) return;
  MultiLineStringBuilder sb;
  fc.DisassembleFunction(options.func_index, sb, OutputMode::kWat);
  sb.DumpToStdout();
}

void HexdumpForFunction(const Options& options) {
  FormatConverter fc(options.filename);
  if (!fc.ok()) return;
  MultiLineStringBuilder sb;
  fc.DisassembleFunction(options.func_index, sb, OutputMode::kHexDump);
  sb.DumpToStdout();
}

void WatForModule(const Options& options) {
  FormatConverter fc(options.filename);
  if (!fc.ok()) return;
  MultiLineStringBuilder sb;
  fc.WatForModule(sb);
  sb.DumpToStdout();
}

bool ParseInt(char* s, int* out) {
  char* end;
  if (s[0] == '\0') return false;
  errno = 0;
  long l = strtol(s, &end, 10);
  if (errno != 0 || *end != '\0' || l > std::numeric_limits<int>::max() ||
      l < std::numeric_limits<int>::min()) {
    return false;
  }
  *out = static_cast<int>(l);
  return true;
}

int ParseOptions(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ||
        strcmp(argv[i], "help") == 0) {
      options->action = Action::kHelp;
    } else if (strcmp(argv[i], "--list-functions") == 0) {
      options->action = Action::kListFunctions;
    } else if (strcmp(argv[i], "--section-stats") == 0) {
      options->action = Action::kSectionStats;
    } else if (strcmp(argv[i], "--full-wat") == 0) {
      options->action = Action::kFullWat;
    } else if (strcmp(argv[i], "--single-wat") == 0) {
      options->action = Action::kSingleWat;
      if (i == argc - 1 || !ParseInt(argv[++i], &options->func_index)) {
        return PrintHelp(argv);
      }
    } else if (strncmp(argv[i], "--single-wat=", 13) == 0) {
      options->action = Action::kSingleWat;
      if (!ParseInt(argv[i] + 13, &options->func_index)) return PrintHelp(argv);
    } else if (strcmp(argv[i], "--single-hexdump") == 0) {
      options->action = Action::kSingleHexdump;
      if (i == argc - 1 || !ParseInt(argv[++i], &options->func_index)) {
        return PrintHelp(argv);
      }
    } else if (strncmp(argv[i], "--single-hexdump=", 17) == 0) {
      if (!ParseInt(argv[i] + 17, &options->func_index)) return PrintHelp(argv);
    } else if (options->filename != nullptr) {
      return PrintHelp(argv);
    } else {
      options->filename = argv[i];
    }
  }
  if (options->action == Action::kUnset || options->filename == nullptr) {
    return PrintHelp(argv);
  }
  return 0;
}

int main(int argc, char** argv) {
  Options options;
  if (ParseOptions(argc, argv, &options) != 0) return 1;
  // Bootstrap the basics.
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  switch (options.action) {
    // clang-format off
    case Action::kHelp:          PrintHelp(argv);             break;
    case Action::kListFunctions: ListFunctions(options);      break;
    case Action::kSectionStats:  SectionStats(options);       break;
    case Action::kSingleWat:     WatForFunction(options);     break;
    case Action::kSingleHexdump: HexdumpForFunction(options); break;
    case Action::kFullWat:       WatForModule(options);       break;
    case Action::kUnset:         UNREACHABLE();
      // clang-format on
  }

  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  return 0;
}
