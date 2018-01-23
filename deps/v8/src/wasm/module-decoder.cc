// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-decoder.h"

#include "src/base/functional.h"
#include "src/base/platform/platform.h"
#include "src/base/template-utils.h"
#include "src/counters.h"
#include "src/flags.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "src/v8.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/wasm-limits.h"

namespace v8 {
namespace internal {
namespace wasm {

#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)

namespace {

constexpr char kNameString[] = "name";
constexpr char kExceptionString[] = "exception";
constexpr char kUnknownString[] = "<unknown>";

template <size_t N>
constexpr size_t num_chars(const char (&)[N]) {
  return N - 1;  // remove null character at end.
}

const char* ExternalKindName(ImportExportKindCode kind) {
  switch (kind) {
    case kExternalFunction:
      return "function";
    case kExternalTable:
      return "table";
    case kExternalMemory:
      return "memory";
    case kExternalGlobal:
      return "global";
  }
  return "unknown";
}

}  // namespace

const char* SectionName(SectionCode code) {
  switch (code) {
    case kUnknownSectionCode:
      return "Unknown";
    case kTypeSectionCode:
      return "Type";
    case kImportSectionCode:
      return "Import";
    case kFunctionSectionCode:
      return "Function";
    case kTableSectionCode:
      return "Table";
    case kMemorySectionCode:
      return "Memory";
    case kGlobalSectionCode:
      return "Global";
    case kExportSectionCode:
      return "Export";
    case kStartSectionCode:
      return "Start";
    case kCodeSectionCode:
      return "Code";
    case kElementSectionCode:
      return "Element";
    case kDataSectionCode:
      return "Data";
    case kNameSectionCode:
      return kNameString;
    case kExceptionSectionCode:
      if (FLAG_experimental_wasm_eh) return kExceptionString;
      return kUnknownString;
    default:
      return kUnknownString;
  }
}

namespace {

ValueType TypeOf(const WasmModule* module, const WasmInitExpr& expr) {
  switch (expr.kind) {
    case WasmInitExpr::kNone:
      return kWasmStmt;
    case WasmInitExpr::kGlobalIndex:
      return expr.val.global_index < module->globals.size()
                 ? module->globals[expr.val.global_index].type
                 : kWasmStmt;
    case WasmInitExpr::kI32Const:
      return kWasmI32;
    case WasmInitExpr::kI64Const:
      return kWasmI64;
    case WasmInitExpr::kF32Const:
      return kWasmF32;
    case WasmInitExpr::kF64Const:
      return kWasmF64;
    default:
      UNREACHABLE();
  }
}

// Reads a length-prefixed string, checking that it is within bounds. Returns
// the offset of the string, and the length as an out parameter.
WireBytesRef consume_string(Decoder& decoder, bool validate_utf8,
                            const char* name) {
  uint32_t length = decoder.consume_u32v("string length");
  uint32_t offset = decoder.pc_offset();
  const byte* string_start = decoder.pc();
  // Consume bytes before validation to guarantee that the string is not oob.
  if (length > 0) {
    decoder.consume_bytes(length, name);
    if (decoder.ok() && validate_utf8 &&
        !unibrow::Utf8::ValidateEncoding(string_start, length)) {
      decoder.errorf(string_start, "%s: no valid UTF-8 string", name);
    }
  }
  return {offset, decoder.failed() ? 0 : length};
}

// An iterator over the sections in a wasm binary module.
// Automatically skips all unknown sections.
class WasmSectionIterator {
 public:
  explicit WasmSectionIterator(Decoder& decoder)
      : decoder_(decoder),
        section_code_(kUnknownSectionCode),
        section_start_(decoder.pc()),
        section_end_(decoder.pc()) {
    next();
  }

  inline bool more() const { return decoder_.ok() && decoder_.more(); }

  inline SectionCode section_code() const { return section_code_; }

  inline const byte* section_start() const { return section_start_; }

  inline uint32_t section_length() const {
    return static_cast<uint32_t>(section_end_ - section_start_);
  }

  inline Vector<const uint8_t> payload() const {
    return {payload_start_, payload_length()};
  }

  inline const byte* payload_start() const { return payload_start_; }

  inline uint32_t payload_length() const {
    return static_cast<uint32_t>(section_end_ - payload_start_);
  }

  inline const byte* section_end() const { return section_end_; }

  // Advances to the next section, checking that decoding the current section
  // stopped at {section_end_}.
  void advance(bool move_to_section_end = false) {
    if (move_to_section_end && decoder_.pc() < section_end_) {
      decoder_.consume_bytes(
          static_cast<uint32_t>(section_end_ - decoder_.pc()));
    }
    if (decoder_.pc() != section_end_) {
      const char* msg = decoder_.pc() < section_end_ ? "shorter" : "longer";
      decoder_.errorf(decoder_.pc(),
                      "section was %s than expected size "
                      "(%u bytes expected, %zu decoded)",
                      msg, section_length(),
                      static_cast<size_t>(decoder_.pc() - section_start_));
    }
    next();
  }

 private:
  Decoder& decoder_;
  SectionCode section_code_;
  const byte* section_start_;
  const byte* payload_start_;
  const byte* section_end_;

  // Reads the section code/name at the current position and sets up
  // the embedder fields.
  void next() {
    if (!decoder_.more()) {
      section_code_ = kUnknownSectionCode;
      return;
    }
    section_start_ = decoder_.pc();
    uint8_t section_code = decoder_.consume_u8("section code");
    // Read and check the section size.
    uint32_t section_length = decoder_.consume_u32v("section length");

    payload_start_ = decoder_.pc();
    if (decoder_.checkAvailable(section_length)) {
      // Get the limit of the section within the module.
      section_end_ = payload_start_ + section_length;
    } else {
      // The section would extend beyond the end of the module.
      section_end_ = payload_start_;
    }

    if (section_code == kUnknownSectionCode) {
      // Check for the known "name" section.
      WireBytesRef string =
          wasm::consume_string(decoder_, true, "section name");
      if (decoder_.failed() || decoder_.pc() > section_end_) {
        section_code_ = kUnknownSectionCode;
        return;
      }
      const byte* section_name_start =
          decoder_.start() + decoder_.GetBufferRelativeOffset(string.offset());
      payload_start_ = decoder_.pc();

      TRACE("  +%d  section name        : \"%.*s\"\n",
            static_cast<int>(section_name_start - decoder_.start()),
            string.length() < 20 ? string.length() : 20, section_name_start);

      if (string.length() == num_chars(kNameString) &&
          strncmp(reinterpret_cast<const char*>(section_name_start),
                  kNameString, num_chars(kNameString)) == 0) {
        section_code = kNameSectionCode;
      }
    } else if (!IsValidSectionCode(section_code)) {
      decoder_.errorf(decoder_.pc(), "unknown section code #0x%02x",
                      section_code);
      section_code = kUnknownSectionCode;
    }
    section_code_ = decoder_.failed() ? kUnknownSectionCode
                                      : static_cast<SectionCode>(section_code);

    if (section_code_ == kUnknownSectionCode && section_end_ > decoder_.pc()) {
      // skip to the end of the unknown section.
      uint32_t remaining = static_cast<uint32_t>(section_end_ - decoder_.pc());
      decoder_.consume_bytes(remaining, "section payload");
    }
  }
};

}  // namespace

// The main logic for decoding the bytes of a module.
class ModuleDecoderImpl : public Decoder {
 public:
  explicit ModuleDecoderImpl(ModuleOrigin origin)
      : Decoder(nullptr, nullptr),
        origin_(FLAG_assume_asmjs_origin ? kAsmJsOrigin : origin) {}

  ModuleDecoderImpl(const byte* module_start, const byte* module_end,
                    ModuleOrigin origin)
      : Decoder(module_start, module_end),
        origin_(FLAG_assume_asmjs_origin ? kAsmJsOrigin : origin) {
    if (end_ < start_) {
      error(start_, "end is less than start");
      end_ = start_;
    }
  }

  virtual void onFirstError() {
    pc_ = end_;  // On error, terminate section decoding loop.
  }

  void DumpModule(const ModuleResult& result) {
    std::string path;
    if (FLAG_dump_wasm_module_path) {
      path = FLAG_dump_wasm_module_path;
      if (path.size() &&
          !base::OS::isDirectorySeparator(path[path.size() - 1])) {
        path += base::OS::DirectorySeparator();
      }
    }
    // File are named `HASH.{ok,failed}.wasm`.
    size_t hash = base::hash_range(start_, end_);
    EmbeddedVector<char, 32> buf;
    SNPrintF(buf, "%016zx.%s.wasm", hash, result.ok() ? "ok" : "failed");
    std::string name(buf.start());
    if (FILE* wasm_file = base::OS::FOpen((path + name).c_str(), "wb")) {
      if (fwrite(start_, end_ - start_, 1, wasm_file) != 1) {
        OFStream os(stderr);
        os << "Error while dumping wasm file" << std::endl;
      }
      fclose(wasm_file);
    }
  }

  void StartDecoding(Isolate* isolate) {
    CHECK_NULL(module_);
    SetCounters(isolate->counters());
    module_.reset(new WasmModule(
        base::make_unique<Zone>(isolate->allocator(), "signatures")));
    module_->initial_pages = 0;
    module_->maximum_pages = 0;
    module_->mem_export = false;
    module_->set_origin(origin_);
  }

  void DecodeModuleHeader(Vector<const uint8_t> bytes, uint8_t offset) {
    if (failed()) return;
    Reset(bytes, offset);

    const byte* pos = pc_;
    uint32_t magic_word = consume_u32("wasm magic");
#define BYTES(x) (x & 0xFF), (x >> 8) & 0xFF, (x >> 16) & 0xFF, (x >> 24) & 0xFF
    if (magic_word != kWasmMagic) {
      errorf(pos,
             "expected magic word %02x %02x %02x %02x, "
             "found %02x %02x %02x %02x",
             BYTES(kWasmMagic), BYTES(magic_word));
    }

    pos = pc_;
    {
      uint32_t magic_version = consume_u32("wasm version");
      if (magic_version != kWasmVersion) {
        errorf(pos,
               "expected version %02x %02x %02x %02x, "
               "found %02x %02x %02x %02x",
               BYTES(kWasmVersion), BYTES(magic_version));
      }
    }
#undef BYTES
  }

  void DecodeSection(SectionCode section_code, Vector<const uint8_t> bytes,
                     uint32_t offset, bool verify_functions = true) {
    if (failed()) return;
    Reset(bytes, offset);
    TRACE("Section: %s\n", SectionName(section_code));
    TRACE("Decode Section %p - %p\n", static_cast<const void*>(bytes.begin()),
          static_cast<const void*>(bytes.end()));

    // Check if the section is out-of-order.
    if (section_code < next_section_) {
      errorf(pc(), "unexpected section: %s", SectionName(section_code));
      return;
    }

    switch (section_code) {
      case kUnknownSectionCode:
        break;
      case kExceptionSectionCode:
        // Note: kExceptionSectionCode > kCodeSectionCode, but must appear
        // before the code section. Hence, treat it as a special case.
        if (++number_of_exception_sections > 1) {
          errorf(pc(), "Multiple exception sections not allowed");
          return;
        } else if (next_section_ >= kCodeSectionCode) {
          errorf(pc(), "Exception section must appear before the code section");
          return;
        }
        break;
      default:
        next_section_ = section_code;
        ++next_section_;
        break;
    }

    switch (section_code) {
      case kUnknownSectionCode:
        break;
      case kTypeSectionCode:
        DecodeTypeSection();
        break;
      case kImportSectionCode:
        DecodeImportSection();
        break;
      case kFunctionSectionCode:
        DecodeFunctionSection();
        break;
      case kTableSectionCode:
        DecodeTableSection();
        break;
      case kMemorySectionCode:
        DecodeMemorySection();
        break;
      case kGlobalSectionCode:
        DecodeGlobalSection();
        break;
      case kExportSectionCode:
        DecodeExportSection();
        break;
      case kStartSectionCode:
        DecodeStartSection();
        break;
      case kCodeSectionCode:
        DecodeCodeSection(verify_functions);
        break;
      case kElementSectionCode:
        DecodeElementSection();
        break;
      case kDataSectionCode:
        DecodeDataSection();
        break;
      case kNameSectionCode:
        DecodeNameSection();
        break;
      case kExceptionSectionCode:
        if (FLAG_experimental_wasm_eh) {
          DecodeExceptionSection();
        } else {
          errorf(pc(), "unexpected section: %s", SectionName(section_code));
        }
        break;
      default:
        errorf(pc(), "unexpected section: %s", SectionName(section_code));
        return;
    }

    if (pc() != bytes.end()) {
      const char* msg = pc() < bytes.end() ? "shorter" : "longer";
      errorf(pc(),
             "section was %s than expected size "
             "(%zu bytes expected, %zu decoded)",
             msg, bytes.size(), static_cast<size_t>(pc() - bytes.begin()));
    }
  }

  void DecodeTypeSection() {
    uint32_t signatures_count = consume_count("types count", kV8MaxWasmTypes);
    module_->signatures.reserve(signatures_count);
    for (uint32_t i = 0; ok() && i < signatures_count; ++i) {
      TRACE("DecodeSignature[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));
      FunctionSig* s = consume_sig(module_->signature_zone.get());
      module_->signatures.push_back(s);
      uint32_t id = s ? module_->signature_map.FindOrInsert(s) : 0;
      module_->signature_ids.push_back(id);
    }
    module_->signature_map.Freeze();
  }

  void DecodeImportSection() {
    uint32_t import_table_count =
        consume_count("imports count", kV8MaxWasmImports);
    module_->import_table.reserve(import_table_count);
    for (uint32_t i = 0; ok() && i < import_table_count; ++i) {
      TRACE("DecodeImportTable[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      module_->import_table.push_back({
          {0, 0},             // module_name
          {0, 0},             // field_name
          kExternalFunction,  // kind
          0                   // index
      });
      WasmImport* import = &module_->import_table.back();
      const byte* pos = pc_;
      import->module_name = consume_string(true, "module name");
      import->field_name = consume_string(true, "field name");
      import->kind =
          static_cast<ImportExportKindCode>(consume_u8("import kind"));
      switch (import->kind) {
        case kExternalFunction: {
          // ===== Imported function =======================================
          import->index = static_cast<uint32_t>(module_->functions.size());
          module_->num_imported_functions++;
          module_->functions.push_back({nullptr,        // sig
                                        import->index,  // func_index
                                        0,              // sig_index
                                        {0, 0},         // name_offset
                                        {0, 0},         // code
                                        true,           // imported
                                        false});        // exported
          WasmFunction* function = &module_->functions.back();
          function->sig_index =
              consume_sig_index(module_.get(), &function->sig);
          break;
        }
        case kExternalTable: {
          // ===== Imported table ==========================================
          if (!AddTable(module_.get())) break;
          import->index =
              static_cast<uint32_t>(module_->function_tables.size());
          module_->function_tables.emplace_back();
          WasmIndirectFunctionTable* table = &module_->function_tables.back();
          table->imported = true;
          expect_u8("element type", kWasmAnyFunctionTypeCode);
          consume_resizable_limits(
              "element count", "elements", FLAG_wasm_max_table_size,
              &table->initial_size, &table->has_maximum_size,
              FLAG_wasm_max_table_size, &table->maximum_size);
          break;
        }
        case kExternalMemory: {
          // ===== Imported memory =========================================
          if (!AddMemory(module_.get())) break;
          consume_resizable_limits(
              "memory", "pages", FLAG_wasm_max_mem_pages,
              &module_->initial_pages, &module_->has_maximum_pages,
              kSpecMaxWasmMemoryPages, &module_->maximum_pages,
              &module_->has_shared_memory);
          break;
        }
        case kExternalGlobal: {
          // ===== Imported global =========================================
          import->index = static_cast<uint32_t>(module_->globals.size());
          module_->globals.push_back(
              {kWasmStmt, false, WasmInitExpr(), 0, true, false});
          WasmGlobal* global = &module_->globals.back();
          global->type = consume_value_type();
          global->mutability = consume_mutability();
          if (global->mutability) {
            error("mutable globals cannot be imported");
          }
          break;
        }
        default:
          errorf(pos, "unknown import kind 0x%02x", import->kind);
          break;
      }
    }
  }

  void DecodeFunctionSection() {
    uint32_t functions_count =
        consume_count("functions count", kV8MaxWasmFunctions);
    (IsWasm() ? GetCounters()->wasm_functions_per_wasm_module()
              : GetCounters()->wasm_functions_per_asm_module())
        ->AddSample(static_cast<int>(functions_count));
    module_->functions.reserve(functions_count);
    module_->num_declared_functions = functions_count;
    for (uint32_t i = 0; ok() && i < functions_count; ++i) {
      uint32_t func_index = static_cast<uint32_t>(module_->functions.size());
      module_->functions.push_back({nullptr,     // sig
                                    func_index,  // func_index
                                    0,           // sig_index
                                    {0, 0},      // name
                                    {0, 0},      // code
                                    false,       // imported
                                    false});     // exported
      WasmFunction* function = &module_->functions.back();
      function->sig_index = consume_sig_index(module_.get(), &function->sig);
    }
  }

  void DecodeTableSection() {
    uint32_t table_count = consume_count("table count", kV8MaxWasmTables);

    for (uint32_t i = 0; ok() && i < table_count; i++) {
      if (!AddTable(module_.get())) break;
      module_->function_tables.emplace_back();
      WasmIndirectFunctionTable* table = &module_->function_tables.back();
      expect_u8("table type", kWasmAnyFunctionTypeCode);
      consume_resizable_limits("table elements", "elements",
                               FLAG_wasm_max_table_size, &table->initial_size,
                               &table->has_maximum_size,
                               FLAG_wasm_max_table_size, &table->maximum_size);
    }
  }

  void DecodeMemorySection() {
    uint32_t memory_count = consume_count("memory count", kV8MaxWasmMemories);

    for (uint32_t i = 0; ok() && i < memory_count; i++) {
      if (!AddMemory(module_.get())) break;
      consume_resizable_limits(
          "memory", "pages", FLAG_wasm_max_mem_pages, &module_->initial_pages,
          &module_->has_maximum_pages, kSpecMaxWasmMemoryPages,
          &module_->maximum_pages, &module_->has_shared_memory);
    }
  }

  void DecodeGlobalSection() {
    uint32_t globals_count = consume_count("globals count", kV8MaxWasmGlobals);
    uint32_t imported_globals = static_cast<uint32_t>(module_->globals.size());
    module_->globals.reserve(imported_globals + globals_count);
    for (uint32_t i = 0; ok() && i < globals_count; ++i) {
      TRACE("DecodeGlobal[%d] module+%d\n", i, static_cast<int>(pc_ - start_));
      // Add an uninitialized global and pass a pointer to it.
      module_->globals.push_back(
          {kWasmStmt, false, WasmInitExpr(), 0, false, false});
      WasmGlobal* global = &module_->globals.back();
      DecodeGlobalInModule(module_.get(), i + imported_globals, global);
    }
    if (ok()) CalculateGlobalOffsets(module_.get());
  }

  void DecodeExportSection() {
    uint32_t export_table_count =
        consume_count("exports count", kV8MaxWasmImports);
    module_->export_table.reserve(export_table_count);
    for (uint32_t i = 0; ok() && i < export_table_count; ++i) {
      TRACE("DecodeExportTable[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      module_->export_table.push_back({
          {0, 0},             // name
          kExternalFunction,  // kind
          0                   // index
      });
      WasmExport* exp = &module_->export_table.back();

      exp->name = consume_string(true, "field name");

      const byte* pos = pc();
      exp->kind = static_cast<ImportExportKindCode>(consume_u8("export kind"));
      switch (exp->kind) {
        case kExternalFunction: {
          WasmFunction* func = nullptr;
          exp->index = consume_func_index(module_.get(), &func);
          module_->num_exported_functions++;
          if (func) func->exported = true;
          break;
        }
        case kExternalTable: {
          WasmIndirectFunctionTable* table = nullptr;
          exp->index = consume_table_index(module_.get(), &table);
          if (table) table->exported = true;
          break;
        }
        case kExternalMemory: {
          uint32_t index = consume_u32v("memory index");
          // TODO(titzer): This should become more regular
          // once we support multiple memories.
          if (!module_->has_memory || index != 0) {
            error("invalid memory index != 0");
          }
          module_->mem_export = true;
          break;
        }
        case kExternalGlobal: {
          WasmGlobal* global = nullptr;
          exp->index = consume_global_index(module_.get(), &global);
          if (global) {
            if (global->mutability) {
              error("mutable globals cannot be exported");
            }
            global->exported = true;
          }
          break;
        }
        default:
          errorf(pos, "invalid export kind 0x%02x", exp->kind);
          break;
      }
    }
    // Check for duplicate exports (except for asm.js).
    if (ok() && origin_ != kAsmJsOrigin && module_->export_table.size() > 1) {
      std::vector<WasmExport> sorted_exports(module_->export_table);

      auto cmp_less = [this](const WasmExport& a, const WasmExport& b) {
        // Return true if a < b.
        if (a.name.length() != b.name.length()) {
          return a.name.length() < b.name.length();
        }
        const byte* left = start() + GetBufferRelativeOffset(a.name.offset());
        const byte* right = start() + GetBufferRelativeOffset(b.name.offset());
        return memcmp(left, right, a.name.length()) < 0;
      };
      std::stable_sort(sorted_exports.begin(), sorted_exports.end(), cmp_less);

      auto it = sorted_exports.begin();
      WasmExport* last = &*it++;
      for (auto end = sorted_exports.end(); it != end; last = &*it++) {
        DCHECK(!cmp_less(*it, *last));  // Vector must be sorted.
        if (!cmp_less(*last, *it)) {
          const byte* pc = start() + GetBufferRelativeOffset(it->name.offset());
          TruncatedUserString<> name(pc, it->name.length());
          errorf(pc, "Duplicate export name '%.*s' for %s %d and %s %d",
                 name.length(), name.start(), ExternalKindName(last->kind),
                 last->index, ExternalKindName(it->kind), it->index);
          break;
        }
      }
    }
  }

  void DecodeStartSection() {
    WasmFunction* func;
    const byte* pos = pc_;
    module_->start_function_index = consume_func_index(module_.get(), &func);
    if (func &&
        (func->sig->parameter_count() > 0 || func->sig->return_count() > 0)) {
      error(pos, "invalid start function: non-zero parameter or return count");
    }
  }

  void DecodeElementSection() {
    uint32_t element_count =
        consume_count("element count", FLAG_wasm_max_table_size);

    if (element_count > 0 && module_->function_tables.size() == 0) {
      error(pc_, "The element section requires a table");
    }
    for (uint32_t i = 0; ok() && i < element_count; ++i) {
      const byte* pos = pc();
      uint32_t table_index = consume_u32v("table index");
      if (table_index != 0) {
        errorf(pos, "illegal table index %u != 0", table_index);
      }
      if (table_index >= module_->function_tables.size()) {
        errorf(pos, "out of bounds table index %u", table_index);
        break;
      }
      WasmInitExpr offset = consume_init_expr(module_.get(), kWasmI32);
      uint32_t num_elem =
          consume_count("number of elements", kV8MaxWasmTableEntries);
      module_->table_inits.emplace_back(table_index, offset);
      WasmTableInit* init = &module_->table_inits.back();
      for (uint32_t j = 0; j < num_elem; j++) {
        WasmFunction* func = nullptr;
        uint32_t index = consume_func_index(module_.get(), &func);
        DCHECK_IMPLIES(ok(), func != nullptr);
        if (!ok()) break;
        DCHECK_EQ(index, func->func_index);
        init->entries.push_back(index);
      }
    }
  }

  void DecodeCodeSection(bool verify_functions) {
    uint32_t pos = pc_offset();
    uint32_t functions_count = consume_u32v("functions count");
    CheckFunctionsCount(functions_count, pos);
    for (uint32_t i = 0; ok() && i < functions_count; ++i) {
      const byte* pos = pc();
      uint32_t size = consume_u32v("body size");
      if (size > kV8MaxWasmFunctionSize) {
        errorf(pos, "size %u > maximum function size %zu", size,
               kV8MaxWasmFunctionSize);
        return;
      }
      uint32_t offset = pc_offset();
      consume_bytes(size, "function body");
      if (failed()) break;
      DecodeFunctionBody(i, size, offset, verify_functions);
    }
  }

  bool CheckFunctionsCount(uint32_t functions_count, uint32_t offset) {
    if (functions_count != module_->num_declared_functions) {
      Reset(nullptr, nullptr, offset);
      errorf(nullptr, "function body count %u mismatch (%u expected)",
             functions_count, module_->num_declared_functions);
      return false;
    }
    return true;
  }

  void DecodeFunctionBody(uint32_t index, uint32_t length, uint32_t offset,
                          bool verify_functions) {
    WasmFunction* function =
        &module_->functions[index + module_->num_imported_functions];
    function->code = {offset, length};
    if (verify_functions) {
      ModuleWireBytes bytes(start_, end_);
      VerifyFunctionBody(module_->signature_zone->allocator(),
                         index + module_->num_imported_functions, bytes,
                         module_.get(), function);
    }
  }

  void DecodeDataSection() {
    uint32_t data_segments_count =
        consume_count("data segments count", kV8MaxWasmDataSegments);
    module_->data_segments.reserve(data_segments_count);
    for (uint32_t i = 0; ok() && i < data_segments_count; ++i) {
      if (!module_->has_memory) {
        error("cannot load data without memory");
        break;
      }
      TRACE("DecodeDataSegment[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));
      module_->data_segments.push_back({
          WasmInitExpr(),  // dest_addr
          {0, 0}           // source
      });
      WasmDataSegment* segment = &module_->data_segments.back();
      DecodeDataSegmentInModule(module_.get(), segment);
    }
  }

  void DecodeNameSection() {
    // TODO(titzer): find a way to report name errors as warnings.
    // Use an inner decoder so that errors don't fail the outer decoder.
    Decoder inner(start_, pc_, end_, buffer_offset_);
    // Decode all name subsections.
    // Be lenient with their order.
    while (inner.ok() && inner.more()) {
      uint8_t name_type = inner.consume_u8("name type");
      if (name_type & 0x80) inner.error("name type if not varuint7");

      uint32_t name_payload_len = inner.consume_u32v("name payload length");
      if (!inner.checkAvailable(name_payload_len)) break;

      // Decode function names, ignore the rest.
      // Local names will be decoded when needed.
      switch (name_type) {
        case NameSectionKindCode::kModule: {
          WireBytesRef name = wasm::consume_string(inner, false, "module name");
          if (inner.ok() && validate_utf8(&inner, name)) module_->name = name;
          break;
        }
        case NameSectionKindCode::kFunction: {
          uint32_t functions_count = inner.consume_u32v("functions count");

          for (; inner.ok() && functions_count > 0; --functions_count) {
            uint32_t function_index = inner.consume_u32v("function index");
            WireBytesRef name =
                wasm::consume_string(inner, false, "function name");

            // Be lenient with errors in the name section: Ignore illegal
            // or out-of-order indexes and non-UTF8 names. You can even assign
            // to the same function multiple times (last valid one wins).
            if (inner.ok() && function_index < module_->functions.size() &&
                validate_utf8(&inner, name)) {
              module_->functions[function_index].name = name;
            }
          }
          break;
        }
        default:
          inner.consume_bytes(name_payload_len, "name subsection payload");
          break;
      }
    }
    // Skip the whole names section in the outer decoder.
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeExceptionSection() {
    uint32_t exception_count =
        consume_count("exception count", kV8MaxWasmExceptions);
    for (uint32_t i = 0; ok() && i < exception_count; ++i) {
      TRACE("DecodeExceptionSignature[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));
      module_->exceptions.emplace_back(
          consume_exception_sig(module_->signature_zone.get()));
    }
  }

  ModuleResult FinishDecoding(bool verify_functions = true) {
    if (ok()) {
      CalculateGlobalOffsets(module_.get());
    }
    ModuleResult result = toResult(std::move(module_));
    if (verify_functions && result.ok()) {
      // Copy error code and location.
      result.MoveErrorFrom(intermediate_result_);
    }
    if (FLAG_dump_wasm_module) DumpModule(result);
    return result;
  }

  // Decodes an entire module.
  ModuleResult DecodeModule(Isolate* isolate, bool verify_functions = true) {
    StartDecoding(isolate);
    uint32_t offset = 0;
    DecodeModuleHeader(Vector<const uint8_t>(start(), end() - start()), offset);
    if (failed()) {
      return FinishDecoding(verify_functions);
    }
    // Size of the module header.
    offset += 8;
    Decoder decoder(start_ + offset, end_, offset);

    WasmSectionIterator section_iter(decoder);

    while (ok() && section_iter.more()) {
      // Shift the offset by the section header length
      offset += section_iter.payload_start() - section_iter.section_start();
      if (section_iter.section_code() != SectionCode::kUnknownSectionCode) {
        DecodeSection(section_iter.section_code(), section_iter.payload(),
                      offset, verify_functions);
      }
      // Shift the offset by the remaining section payload
      offset += section_iter.payload_length();
      section_iter.advance(true);
    }

    if (decoder.failed()) {
      return decoder.toResult<std::unique_ptr<WasmModule>>(nullptr);
    }

    return FinishDecoding(verify_functions);
  }

  // Decodes a single anonymous function starting at {start_}.
  FunctionResult DecodeSingleFunction(Zone* zone,
                                      const ModuleWireBytes& wire_bytes,
                                      const WasmModule* module,
                                      std::unique_ptr<WasmFunction> function) {
    pc_ = start_;
    function->sig = consume_sig(zone);
    function->name = {0, 0};
    function->code = {off(pc_), static_cast<uint32_t>(end_ - pc_)};

    if (ok())
      VerifyFunctionBody(zone->allocator(), 0, wire_bytes, module,
                         function.get());

    FunctionResult result(std::move(function));
    // Copy error code and location.
    result.MoveErrorFrom(intermediate_result_);
    return result;
  }

  // Decodes a single function signature at {start}.
  FunctionSig* DecodeFunctionSignature(Zone* zone, const byte* start) {
    pc_ = start;
    FunctionSig* result = consume_sig(zone);
    return ok() ? result : nullptr;
  }

  WasmInitExpr DecodeInitExpr(const byte* start) {
    pc_ = start;
    return consume_init_expr(nullptr, kWasmStmt);
  }

  WasmModule* module() { return module_.get(); }

  bool IsWasm() { return origin_ == kWasmOrigin; }

  Counters* GetCounters() {
    DCHECK_NOT_NULL(counters_);
    return counters_;
  }

  void SetCounters(Counters* counters) {
    DCHECK_NULL(counters_);
    counters_ = counters;
  }

 private:
  std::unique_ptr<WasmModule> module_;
  Counters* counters_ = nullptr;
  // The type section is the first section in a module.
  uint8_t next_section_ = kFirstSectionInModule;
  uint32_t number_of_exception_sections = 0;
  // We store next_section_ as uint8_t instead of SectionCode so that we can
  // increment it. This static_assert should make sure that SectionCode does not
  // get bigger than uint8_t accidentially.
  static_assert(sizeof(ModuleDecoderImpl::next_section_) == sizeof(SectionCode),
                "type mismatch");
  Result<bool> intermediate_result_;
  ModuleOrigin origin_;

  uint32_t off(const byte* ptr) {
    return static_cast<uint32_t>(ptr - start_) + buffer_offset_;
  }

  bool AddTable(WasmModule* module) {
    if (module->function_tables.size() > 0) {
      error("At most one table is supported");
      return false;
    } else {
      return true;
    }
  }

  bool AddMemory(WasmModule* module) {
    if (module->has_memory) {
      error("At most one memory is supported");
      return false;
    } else {
      module->has_memory = true;
      return true;
    }
  }

  // Decodes a single global entry inside a module starting at {pc_}.
  void DecodeGlobalInModule(WasmModule* module, uint32_t index,
                            WasmGlobal* global) {
    global->type = consume_value_type();
    global->mutability = consume_mutability();
    const byte* pos = pc();
    global->init = consume_init_expr(module, kWasmStmt);
    switch (global->init.kind) {
      case WasmInitExpr::kGlobalIndex: {
        uint32_t other_index = global->init.val.global_index;
        if (other_index >= index) {
          errorf(pos,
                 "invalid global index in init expression, "
                 "index %u, other_index %u",
                 index, other_index);
        } else if (module->globals[other_index].type != global->type) {
          errorf(pos,
                 "type mismatch in global initialization "
                 "(from global #%u), expected %s, got %s",
                 other_index, WasmOpcodes::TypeName(global->type),
                 WasmOpcodes::TypeName(module->globals[other_index].type));
        }
        break;
      }
      default:
        if (global->type != TypeOf(module, global->init)) {
          errorf(pos,
                 "type error in global initialization, expected %s, got %s",
                 WasmOpcodes::TypeName(global->type),
                 WasmOpcodes::TypeName(TypeOf(module, global->init)));
        }
    }
  }

  // Decodes a single data segment entry inside a module starting at {pc_}.
  void DecodeDataSegmentInModule(WasmModule* module, WasmDataSegment* segment) {
    expect_u8("linear memory index", 0);
    segment->dest_addr = consume_init_expr(module, kWasmI32);
    uint32_t source_length = consume_u32v("source size");
    uint32_t source_offset = pc_offset();

    consume_bytes(source_length, "segment data");
    if (failed()) return;

    segment->source = {source_offset, source_length};
  }

  // Calculate individual global offsets and total size of globals table.
  void CalculateGlobalOffsets(WasmModule* module) {
    uint32_t offset = 0;
    if (module->globals.size() == 0) {
      module->globals_size = 0;
      return;
    }
    for (WasmGlobal& global : module->globals) {
      byte size =
          WasmOpcodes::MemSize(WasmOpcodes::MachineTypeFor(global.type));
      offset = (offset + size - 1) & ~(size - 1);  // align
      global.offset = offset;
      offset += size;
    }
    module->globals_size = offset;
  }

  // Verifies the body (code) of a given function.
  void VerifyFunctionBody(AccountingAllocator* allocator, uint32_t func_num,
                          const ModuleWireBytes& wire_bytes,
                          const WasmModule* module, WasmFunction* function) {
    WasmFunctionName func_name(function, wire_bytes.GetNameOrNull(function));
    if (FLAG_trace_wasm_decoder || FLAG_trace_wasm_decode_time) {
      OFStream os(stdout);
      os << "Verifying wasm function " << func_name << std::endl;
    }
    FunctionBody body = {
        function->sig, function->code.offset(),
        start_ + GetBufferRelativeOffset(function->code.offset()),
        start_ + GetBufferRelativeOffset(function->code.end_offset())};
    DecodeResult result = VerifyWasmCodeWithStats(allocator, module, body,
                                                  IsWasm(), GetCounters());
    if (result.failed()) {
      // Wrap the error message from the function decoder.
      std::ostringstream wrapped;
      wrapped << "in function " << func_name << ": " << result.error_msg();
      result.error(result.error_offset(), wrapped.str());

      // Set error code and location, if this is the first error.
      if (intermediate_result_.ok()) {
        intermediate_result_.MoveErrorFrom(result);
      }
    }
  }

  WireBytesRef consume_string(bool validate_utf8, const char* name) {
    return wasm::consume_string(*this, validate_utf8, name);
  }

  bool validate_utf8(Decoder* decoder, WireBytesRef string) {
    return unibrow::Utf8::ValidateEncoding(
        decoder->start() + decoder->GetBufferRelativeOffset(string.offset()),
        string.length());
  }

  uint32_t consume_sig_index(WasmModule* module, FunctionSig** sig) {
    const byte* pos = pc_;
    uint32_t sig_index = consume_u32v("signature index");
    if (sig_index >= module->signatures.size()) {
      errorf(pos, "signature index %u out of bounds (%d signatures)", sig_index,
             static_cast<int>(module->signatures.size()));
      *sig = nullptr;
      return 0;
    }
    *sig = module->signatures[sig_index];
    return sig_index;
  }

  uint32_t consume_count(const char* name, size_t maximum) {
    const byte* p = pc_;
    uint32_t count = consume_u32v(name);
    if (count > maximum) {
      errorf(p, "%s of %u exceeds internal limit of %zu", name, count, maximum);
      return static_cast<uint32_t>(maximum);
    }
    return count;
  }

  uint32_t consume_func_index(WasmModule* module, WasmFunction** func) {
    return consume_index("function index", module->functions, func);
  }

  uint32_t consume_global_index(WasmModule* module, WasmGlobal** global) {
    return consume_index("global index", module->globals, global);
  }

  uint32_t consume_table_index(WasmModule* module,
                               WasmIndirectFunctionTable** table) {
    return consume_index("table index", module->function_tables, table);
  }

  template <typename T>
  uint32_t consume_index(const char* name, std::vector<T>& vector, T** ptr) {
    const byte* pos = pc_;
    uint32_t index = consume_u32v(name);
    if (index >= vector.size()) {
      errorf(pos, "%s %u out of bounds (%d entr%s)", name, index,
             static_cast<int>(vector.size()), vector.size() == 1 ? "y" : "ies");
      *ptr = nullptr;
      return 0;
    }
    *ptr = &vector[index];
    return index;
  }

  void consume_resizable_limits(const char* name, const char* units,
                                uint32_t max_initial, uint32_t* initial,
                                bool* has_max, uint32_t max_maximum,
                                uint32_t* maximum,
                                bool* has_shared_memory = nullptr) {
    uint8_t flags = consume_u8("resizable limits flags");
    const byte* pos = pc();

    if (FLAG_experimental_wasm_threads) {
      bool is_memory = (strcmp(name, "memory") == 0);
      if (flags & 0xFC || (!is_memory && (flags & 0xFE))) {
        errorf(pos - 1, "invalid %s limits flags", name);
      }
      if (flags == 3) {
        DCHECK_NOT_NULL(has_shared_memory);
        *has_shared_memory = true;
      } else if (flags == 2) {
        errorf(pos - 1,
               "%s limits flags should have maximum defined if shared is true",
               name);
      }
    } else {
      if (flags & 0xFE) {
        errorf(pos - 1, "invalid %s limits flags", name);
      }
    }

    *initial = consume_u32v("initial size");
    *has_max = false;
    if (*initial > max_initial) {
      errorf(pos,
             "initial %s size (%u %s) is larger than implementation limit (%u)",
             name, *initial, units, max_initial);
    }
    if (flags & 1) {
      *has_max = true;
      pos = pc();
      *maximum = consume_u32v("maximum size");
      if (*maximum > max_maximum) {
        errorf(
            pos,
            "maximum %s size (%u %s) is larger than implementation limit (%u)",
            name, *maximum, units, max_maximum);
      }
      if (*maximum < *initial) {
        errorf(pos, "maximum %s size (%u %s) is less than initial (%u %s)",
               name, *maximum, units, *initial, units);
      }
    } else {
      *has_max = false;
      *maximum = max_initial;
    }
  }

  bool expect_u8(const char* name, uint8_t expected) {
    const byte* pos = pc();
    uint8_t value = consume_u8(name);
    if (value != expected) {
      errorf(pos, "expected %s 0x%02x, got 0x%02x", name, expected, value);
      return false;
    }
    return true;
  }

  WasmInitExpr consume_init_expr(WasmModule* module, ValueType expected) {
    const byte* pos = pc();
    uint8_t opcode = consume_u8("opcode");
    WasmInitExpr expr;
    unsigned len = 0;
    switch (opcode) {
      case kExprGetGlobal: {
        GlobalIndexOperand<Decoder::kValidate> operand(this, pc() - 1);
        if (module->globals.size() <= operand.index) {
          error("global index is out of bounds");
          expr.kind = WasmInitExpr::kNone;
          expr.val.i32_const = 0;
          break;
        }
        WasmGlobal* global = &module->globals[operand.index];
        if (global->mutability || !global->imported) {
          error(
              "only immutable imported globals can be used in initializer "
              "expressions");
          expr.kind = WasmInitExpr::kNone;
          expr.val.i32_const = 0;
          break;
        }
        expr.kind = WasmInitExpr::kGlobalIndex;
        expr.val.global_index = operand.index;
        len = operand.length;
        break;
      }
      case kExprI32Const: {
        ImmI32Operand<Decoder::kValidate> operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kI32Const;
        expr.val.i32_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprF32Const: {
        ImmF32Operand<Decoder::kValidate> operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kF32Const;
        expr.val.f32_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprI64Const: {
        ImmI64Operand<Decoder::kValidate> operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kI64Const;
        expr.val.i64_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprF64Const: {
        ImmF64Operand<Decoder::kValidate> operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kF64Const;
        expr.val.f64_const = operand.value;
        len = operand.length;
        break;
      }
      default: {
        error("invalid opcode in initialization expression");
        expr.kind = WasmInitExpr::kNone;
        expr.val.i32_const = 0;
      }
    }
    consume_bytes(len, "init code");
    if (!expect_u8("end opcode", kExprEnd)) {
      expr.kind = WasmInitExpr::kNone;
    }
    if (expected != kWasmStmt && TypeOf(module, expr) != kWasmI32) {
      errorf(pos, "type error in init expression, expected %s, got %s",
             WasmOpcodes::TypeName(expected),
             WasmOpcodes::TypeName(TypeOf(module, expr)));
    }
    return expr;
  }

  // Read a mutability flag
  bool consume_mutability() {
    byte val = consume_u8("mutability");
    if (val > 1) error(pc_ - 1, "invalid mutability");
    return val != 0;
  }

  // Reads a single 8-bit integer, interpreting it as a local type.
  ValueType consume_value_type() {
    byte val = consume_u8("value type");
    ValueTypeCode t = static_cast<ValueTypeCode>(val);
    switch (t) {
      case kLocalI32:
        return kWasmI32;
      case kLocalI64:
        return kWasmI64;
      case kLocalF32:
        return kWasmF32;
      case kLocalF64:
        return kWasmF64;
      default:
        if (IsWasm() && FLAG_experimental_wasm_simd) {
          switch (t) {
            case kLocalS128:
              return kWasmS128;
            default:
              break;
          }
        }
        error(pc_ - 1, "invalid local type");
        return kWasmStmt;
    }
  }

  FunctionSig* consume_sig(Zone* zone) {
    constexpr bool has_return_values = true;
    return consume_sig_internal(zone, has_return_values);
  }

  WasmExceptionSig* consume_exception_sig(Zone* zone) {
    constexpr bool has_return_values = true;
    return consume_sig_internal(zone, !has_return_values);
  }

 private:
  FunctionSig* consume_sig_internal(Zone* zone, bool has_return_values) {
    if (has_return_values && !expect_u8("type form", kWasmFunctionTypeCode))
      return nullptr;
    // parse parameter types
    uint32_t param_count =
        consume_count("param count", kV8MaxWasmFunctionParams);
    if (failed()) return nullptr;
    std::vector<ValueType> params;
    for (uint32_t i = 0; ok() && i < param_count; ++i) {
      ValueType param = consume_value_type();
      params.push_back(param);
    }
    std::vector<ValueType> returns;
    uint32_t return_count = 0;
    if (has_return_values) {
      // parse return types
      const size_t max_return_count = FLAG_experimental_wasm_mv
                                          ? kV8MaxWasmFunctionMultiReturns
                                          : kV8MaxWasmFunctionReturns;
      return_count = consume_count("return count", max_return_count);
      if (failed()) return nullptr;
      for (uint32_t i = 0; ok() && i < return_count; ++i) {
        ValueType ret = consume_value_type();
        returns.push_back(ret);
      }
    }

    if (failed()) return nullptr;

    // FunctionSig stores the return types first.
    ValueType* buffer = zone->NewArray<ValueType>(param_count + return_count);
    uint32_t b = 0;
    for (uint32_t i = 0; i < return_count; ++i) buffer[b++] = returns[i];
    for (uint32_t i = 0; i < param_count; ++i) buffer[b++] = params[i];

    return new (zone) FunctionSig(return_count, param_count, buffer);
  }
};

ModuleResult DecodeWasmModule(Isolate* isolate, const byte* module_start,
                              const byte* module_end, bool verify_functions,
                              ModuleOrigin origin, Counters* counters) {
  auto counter = origin == kWasmOrigin
                     ? counters->wasm_decode_wasm_module_time()
                     : counters->wasm_decode_asm_module_time();
  TimedHistogramScope wasm_decode_module_time_scope(counter);
  size_t size = module_end - module_start;
  if (module_start > module_end) return ModuleResult::Error("start > end");
  if (size >= kV8MaxWasmModuleSize)
    return ModuleResult::Error("size > maximum module size: %zu", size);
  // TODO(bradnelson): Improve histogram handling of size_t.
  auto size_counter = origin == kWasmOrigin
                          ? counters->wasm_wasm_module_size_bytes()
                          : counters->wasm_asm_module_size_bytes();
  size_counter->AddSample(static_cast<int>(size));
  // Signatures are stored in zone memory, which have the same lifetime
  // as the {module}.
  ModuleDecoderImpl decoder(module_start, module_end, origin);
  ModuleResult result = decoder.DecodeModule(isolate, verify_functions);
  // TODO(bradnelson): Improve histogram handling of size_t.
  // TODO(titzer): this isn't accurate, since it doesn't count the data
  // allocated on the C++ heap.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=657320
  if (result.ok()) {
    auto peak_counter =
        origin == kWasmOrigin
            ? counters->wasm_decode_wasm_module_peak_memory_bytes()
            : counters->wasm_decode_asm_module_peak_memory_bytes();
    peak_counter->AddSample(
        static_cast<int>(result.val->signature_zone->allocation_size()));
  }
  return result;
}

ModuleDecoder::ModuleDecoder() = default;
ModuleDecoder::~ModuleDecoder() = default;

WasmModule* ModuleDecoder::module() const { return impl_->module(); }

void ModuleDecoder::StartDecoding(Isolate* isolate, ModuleOrigin origin) {
  DCHECK_NULL(impl_);
  impl_.reset(new ModuleDecoderImpl(origin));
  impl_->StartDecoding(isolate);
}

void ModuleDecoder::DecodeModuleHeader(Vector<const uint8_t> bytes,
                                       uint32_t offset) {
  impl_->DecodeModuleHeader(bytes, offset);
}

void ModuleDecoder::DecodeSection(SectionCode section_code,
                                  Vector<const uint8_t> bytes, uint32_t offset,
                                  bool verify_functions) {
  impl_->DecodeSection(section_code, bytes, offset, verify_functions);
}

void ModuleDecoder::DecodeFunctionBody(uint32_t index, uint32_t length,
                                       uint32_t offset, bool verify_functions) {
  impl_->DecodeFunctionBody(index, length, offset, verify_functions);
}

bool ModuleDecoder::CheckFunctionsCount(uint32_t functions_count,
                                        uint32_t offset) {
  return impl_->CheckFunctionsCount(functions_count, offset);
}

ModuleResult ModuleDecoder::FinishDecoding(bool verify_functions) {
  return impl_->FinishDecoding(verify_functions);
}

bool ModuleDecoder::ok() { return impl_->ok(); }

ModuleResult SyncDecodeWasmModule(Isolate* isolate, const byte* module_start,
                                  const byte* module_end, bool verify_functions,
                                  ModuleOrigin origin) {
  return DecodeWasmModule(isolate, module_start, module_end, verify_functions,
                          origin, isolate->counters());
}

ModuleResult AsyncDecodeWasmModule(
    Isolate* isolate, const byte* module_start, const byte* module_end,
    bool verify_functions, ModuleOrigin origin,
    const std::shared_ptr<Counters> async_counters) {
  return DecodeWasmModule(isolate, module_start, module_end, verify_functions,
                          origin, async_counters.get());
}

FunctionSig* DecodeWasmSignatureForTesting(Zone* zone, const byte* start,
                                           const byte* end) {
  ModuleDecoderImpl decoder(start, end, kWasmOrigin);
  return decoder.DecodeFunctionSignature(zone, start);
}

WasmInitExpr DecodeWasmInitExprForTesting(const byte* start, const byte* end) {
  AccountingAllocator allocator;
  ModuleDecoderImpl decoder(start, end, kWasmOrigin);
  return decoder.DecodeInitExpr(start);
}

namespace {

FunctionResult DecodeWasmFunction(Isolate* isolate, Zone* zone,
                                  const ModuleWireBytes& wire_bytes,
                                  const WasmModule* module,
                                  const byte* function_start,
                                  const byte* function_end,
                                  Counters* counters) {
  size_t size = function_end - function_start;
  if (function_start > function_end)
    return FunctionResult::Error("start > end");
  auto size_histogram = module->is_wasm()
                            ? counters->wasm_wasm_function_size_bytes()
                            : counters->wasm_asm_function_size_bytes();
  // TODO(bradnelson): Improve histogram handling of ptrdiff_t.
  size_histogram->AddSample(static_cast<int>(size));
  if (size > kV8MaxWasmFunctionSize)
    return FunctionResult::Error("size > maximum function size: %zu", size);
  ModuleDecoderImpl decoder(function_start, function_end, kWasmOrigin);
  decoder.SetCounters(counters);
  return decoder.DecodeSingleFunction(zone, wire_bytes, module,
                                      base::make_unique<WasmFunction>());
}

}  // namespace

FunctionResult SyncDecodeWasmFunction(Isolate* isolate, Zone* zone,
                                      const ModuleWireBytes& wire_bytes,
                                      const WasmModule* module,
                                      const byte* function_start,
                                      const byte* function_end) {
  return DecodeWasmFunction(isolate, zone, wire_bytes, module, function_start,
                            function_end, isolate->counters());
}

FunctionResult AsyncDecodeWasmFunction(
    Isolate* isolate, Zone* zone, const ModuleWireBytes& wire_bytes,
    const WasmModule* module, const byte* function_start,
    const byte* function_end, std::shared_ptr<Counters> async_counters) {
  return DecodeWasmFunction(isolate, zone, wire_bytes, module, function_start,
                            function_end, async_counters.get());
}

AsmJsOffsetsResult DecodeAsmJsOffsets(const byte* tables_start,
                                      const byte* tables_end) {
  AsmJsOffsets table;

  Decoder decoder(tables_start, tables_end);
  uint32_t functions_count = decoder.consume_u32v("functions count");
  // Reserve space for the entries, taking care of invalid input.
  if (functions_count < static_cast<unsigned>(tables_end - tables_start)) {
    table.reserve(functions_count);
  }

  for (uint32_t i = 0; i < functions_count && decoder.ok(); ++i) {
    uint32_t size = decoder.consume_u32v("table size");
    if (size == 0) {
      table.emplace_back();
      continue;
    }
    if (!decoder.checkAvailable(size)) {
      decoder.error("illegal asm function offset table size");
    }
    const byte* table_end = decoder.pc() + size;
    uint32_t locals_size = decoder.consume_u32v("locals size");
    int function_start_position = decoder.consume_u32v("function start pos");
    int last_byte_offset = locals_size;
    int last_asm_position = function_start_position;
    std::vector<AsmJsOffsetEntry> func_asm_offsets;
    func_asm_offsets.reserve(size / 4);  // conservative estimation
    // Add an entry for the stack check, associated with position 0.
    func_asm_offsets.push_back(
        {0, function_start_position, function_start_position});
    while (decoder.ok() && decoder.pc() < table_end) {
      last_byte_offset += decoder.consume_u32v("byte offset delta");
      int call_position =
          last_asm_position + decoder.consume_i32v("call position delta");
      int to_number_position =
          call_position + decoder.consume_i32v("to_number position delta");
      last_asm_position = to_number_position;
      func_asm_offsets.push_back(
          {last_byte_offset, call_position, to_number_position});
    }
    if (decoder.pc() != table_end) {
      decoder.error("broken asm offset table");
    }
    table.push_back(std::move(func_asm_offsets));
  }
  if (decoder.more()) decoder.error("unexpected additional bytes");

  return decoder.toResult(std::move(table));
}

std::vector<CustomSectionOffset> DecodeCustomSections(const byte* start,
                                                      const byte* end) {
  Decoder decoder(start, end);
  decoder.consume_bytes(4, "wasm magic");
  decoder.consume_bytes(4, "wasm version");

  std::vector<CustomSectionOffset> result;

  while (decoder.more()) {
    byte section_code = decoder.consume_u8("section code");
    uint32_t section_length = decoder.consume_u32v("section length");
    uint32_t section_start = decoder.pc_offset();
    if (section_code != 0) {
      // Skip known sections.
      decoder.consume_bytes(section_length, "section bytes");
      continue;
    }
    uint32_t name_length = decoder.consume_u32v("name length");
    uint32_t name_offset = decoder.pc_offset();
    decoder.consume_bytes(name_length, "section name");
    uint32_t payload_offset = decoder.pc_offset();
    if (section_length < (payload_offset - section_start)) {
      decoder.error("invalid section length");
      break;
    }
    uint32_t payload_length = section_length - (payload_offset - section_start);
    decoder.consume_bytes(payload_length);
    if (decoder.failed()) break;
    result.push_back({{section_start, section_length},
                      {name_offset, name_length},
                      {payload_offset, payload_length}});
  }

  return result;
}

void DecodeLocalNames(const byte* module_start, const byte* module_end,
                      LocalNames* result) {
  DCHECK_NOT_NULL(result);
  DCHECK(result->names.empty());

  static constexpr int kModuleHeaderSize = 8;
  Decoder decoder(module_start, module_end);
  decoder.consume_bytes(kModuleHeaderSize, "module header");

  WasmSectionIterator section_iter(decoder);

  while (decoder.ok() && section_iter.more() &&
         section_iter.section_code() != kNameSectionCode) {
    section_iter.advance(true);
  }
  if (!section_iter.more()) return;

  // Reset the decoder to not read beyond the name section end.
  decoder.Reset(section_iter.payload(), decoder.pc_offset());

  while (decoder.ok() && decoder.more()) {
    uint8_t name_type = decoder.consume_u8("name type");
    if (name_type & 0x80) break;  // no varuint7

    uint32_t name_payload_len = decoder.consume_u32v("name payload length");
    if (!decoder.checkAvailable(name_payload_len)) break;

    if (name_type != NameSectionKindCode::kLocal) {
      decoder.consume_bytes(name_payload_len, "name subsection payload");
      continue;
    }

    uint32_t local_names_count = decoder.consume_u32v("local names count");
    for (uint32_t i = 0; i < local_names_count; ++i) {
      uint32_t func_index = decoder.consume_u32v("function index");
      if (func_index > kMaxInt) continue;
      result->names.emplace_back(static_cast<int>(func_index));
      LocalNamesPerFunction& func_names = result->names.back();
      result->max_function_index =
          std::max(result->max_function_index, func_names.function_index);
      uint32_t num_names = decoder.consume_u32v("namings count");
      for (uint32_t k = 0; k < num_names; ++k) {
        uint32_t local_index = decoder.consume_u32v("local index");
        WireBytesRef name = wasm::consume_string(decoder, true, "local name");
        if (!decoder.ok()) break;
        if (local_index > kMaxInt) continue;
        func_names.max_local_index =
            std::max(func_names.max_local_index, static_cast<int>(local_index));
        func_names.names.emplace_back(static_cast<int>(local_index), name);
      }
    }
  }
}

#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
