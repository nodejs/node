// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-decoder.h"
#include "src/wasm/function-body-decoder-impl.h"

#include "src/base/functional.h"
#include "src/base/platform/platform.h"
#include "src/counters.h"
#include "src/flags.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "src/v8.h"

#include "src/wasm/decoder.h"
#include "src/wasm/wasm-limits.h"

namespace v8 {
namespace internal {
namespace wasm {

#if DEBUG
#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif

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
      return "Name";
    default:
      return "<unknown>";
  }
}

namespace {

const char* kNameString = "name";
const size_t kNameStringLength = 4;

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
      return kWasmStmt;
  }
}

// Reads a length-prefixed string, checking that it is within bounds. Returns
// the offset of the string, and the length as an out parameter.
uint32_t consume_string(Decoder& decoder, uint32_t* length, bool validate_utf8,
                        const char* name) {
  *length = decoder.consume_u32v("string length");
  uint32_t offset = decoder.pc_offset();
  const byte* string_start = decoder.pc();
  // Consume bytes before validation to guarantee that the string is not oob.
  if (*length > 0) {
    decoder.consume_bytes(*length, name);
    if (decoder.ok() && validate_utf8 &&
        !unibrow::Utf8::Validate(string_start, *length)) {
      decoder.errorf(string_start, "%s: no valid UTF-8 string", name);
    }
  }
  return offset;
}

// An iterator over the sections in a WASM binary module.
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
      uint32_t string_length;
      uint32_t string_offset =
          wasm::consume_string(decoder_, &string_length, true, "section name");
      if (decoder_.failed() || decoder_.pc() > section_end_) {
        section_code_ = kUnknownSectionCode;
        return;
      }
      const byte* section_name_start =
          decoder_.start() + decoder_.GetBufferRelativeOffset(string_offset);
      payload_start_ = decoder_.pc();

      TRACE("  +%d  section name        : \"%.*s\"\n",
            static_cast<int>(section_name_start - decoder_.start()),
            string_length < 20 ? string_length : 20, section_name_start);

      if (string_length == kNameStringLength &&
          strncmp(reinterpret_cast<const char*>(section_name_start),
                  kNameString, kNameStringLength) == 0) {
        section_code = kNameSectionCode;
      }
    } else if (!IsValidSectionCode(section_code)) {
      decoder_.errorf(decoder_.pc(), "unknown section code #0x%02x",
                      section_code);
      section_code = kUnknownSectionCode;
    }
    section_code_ = decoder_.failed() ? kUnknownSectionCode
                                      : static_cast<SectionCode>(section_code);

    TRACE("Section: %s\n", SectionName(section_code_));
    if (section_code_ == kUnknownSectionCode && section_end_ > decoder_.pc()) {
      // skip to the end of the unknown section.
      uint32_t remaining = static_cast<uint32_t>(section_end_ - decoder_.pc());
      decoder_.consume_bytes(remaining, "section payload");
    }
  }
};

// The main logic for decoding the bytes of a module.
class ModuleDecoder : public Decoder {
 public:
  ModuleDecoder(const byte* module_start, const byte* module_end,
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
    char buf[32] = {'\0'};
#if V8_OS_WIN && _MSC_VER < 1900
#define snprintf sprintf_s
#endif
    snprintf(buf, sizeof(buf) - 1, "%016zx.%s.wasm", hash,
             result.ok() ? "ok" : "failed");
    std::string name(buf);
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
    module_.reset(new WasmModule(
        std::unique_ptr<Zone>(new Zone(isolate->allocator(), "signatures"))));
    module_->min_mem_pages = 0;
    module_->max_mem_pages = 0;
    module_->mem_export = false;
    module_->set_origin(origin_);
  }

  void DecodeModuleHeader(Vector<const uint8_t> bytes, uint8_t offset) {
    if (failed()) return;
    Reset(bytes, offset);

    const byte* pos = pc_;
    uint32_t magic_word = consume_u32("wasm magic");
#define BYTES(x) (x & 0xff), (x >> 8) & 0xff, (x >> 16) & 0xff, (x >> 24) & 0xff
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
  }

  void DecodeSection(SectionCode section_code, Vector<const uint8_t> bytes,
                     uint32_t offset, bool verify_functions = true) {
    if (failed()) return;
    Reset(bytes, offset);

    // Check if the section is out-of-order.
    if (section_code < next_section_) {
      errorf(pc(), "unexpected section: %s", SectionName(section_code));
      return;
    }
    if (section_code != kUnknownSectionCode) {
      next_section_ = section_code;
      ++next_section_;
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
    }
  }

  void DecodeImportSection() {
    uint32_t import_table_count =
        consume_count("imports count", kV8MaxWasmImports);
    module_->import_table.reserve(import_table_count);
    for (uint32_t i = 0; ok() && i < import_table_count; ++i) {
      TRACE("DecodeImportTable[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      module_->import_table.push_back({
          0,                  // module_name_length
          0,                  // module_name_offset
          0,                  // field_name_offset
          0,                  // field_name_length
          kExternalFunction,  // kind
          0                   // index
      });
      WasmImport* import = &module_->import_table.back();
      const byte* pos = pc_;
      import->module_name_offset =
          consume_string(&import->module_name_length, true, "module name");
      import->field_name_offset =
          consume_string(&import->field_name_length, true, "field name");
      import->kind = static_cast<WasmExternalKind>(consume_u8("import kind"));
      switch (import->kind) {
        case kExternalFunction: {
          // ===== Imported function =======================================
          import->index = static_cast<uint32_t>(module_->functions.size());
          module_->num_imported_functions++;
          module_->functions.push_back({nullptr,        // sig
                                        import->index,  // func_index
                                        0,              // sig_index
                                        0,              // name_offset
                                        0,              // name_length
                                        0,              // code_start_offset
                                        0,              // code_end_offset
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
          module_->function_tables.push_back({0, 0, false,
                                              std::vector<int32_t>(), true,
                                              false, SignatureMap()});
          expect_u8("element type", kWasmAnyFunctionTypeForm);
          WasmIndirectFunctionTable* table = &module_->function_tables.back();
          consume_resizable_limits("element count", "elements",
                                   FLAG_wasm_max_table_size, &table->min_size,
                                   &table->has_max, FLAG_wasm_max_table_size,
                                   &table->max_size);
          break;
        }
        case kExternalMemory: {
          // ===== Imported memory =========================================
          if (!AddMemory(module_.get())) break;
          consume_resizable_limits(
              "memory", "pages", FLAG_wasm_max_mem_pages,
              &module_->min_mem_pages, &module_->has_max_mem,
              kSpecMaxWasmMemoryPages, &module_->max_mem_pages);
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
    module_->functions.reserve(functions_count);
    module_->num_declared_functions = functions_count;
    for (uint32_t i = 0; ok() && i < functions_count; ++i) {
      uint32_t func_index = static_cast<uint32_t>(module_->functions.size());
      module_->functions.push_back({nullptr,     // sig
                                    func_index,  // func_index
                                    0,           // sig_index
                                    0,           // name_offset
                                    0,           // name_length
                                    0,           // code_start_offset
                                    0,           // code_end_offset
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
      module_->function_tables.push_back(
          {0, 0, false, std::vector<int32_t>(), false, false, SignatureMap()});
      WasmIndirectFunctionTable* table = &module_->function_tables.back();
      expect_u8("table type", kWasmAnyFunctionTypeForm);
      consume_resizable_limits("table elements", "elements",
                               FLAG_wasm_max_table_size, &table->min_size,
                               &table->has_max, FLAG_wasm_max_table_size,
                               &table->max_size);
    }
  }

  void DecodeMemorySection() {
    uint32_t memory_count = consume_count("memory count", kV8MaxWasmMemories);

    for (uint32_t i = 0; ok() && i < memory_count; i++) {
      if (!AddMemory(module_.get())) break;
      consume_resizable_limits("memory", "pages", FLAG_wasm_max_mem_pages,
                               &module_->min_mem_pages, &module_->has_max_mem,
                               kSpecMaxWasmMemoryPages,
                               &module_->max_mem_pages);
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
  }

  void DecodeExportSection() {
    uint32_t export_table_count =
        consume_count("exports count", kV8MaxWasmImports);
    module_->export_table.reserve(export_table_count);
    for (uint32_t i = 0; ok() && i < export_table_count; ++i) {
      TRACE("DecodeExportTable[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      module_->export_table.push_back({
          0,                  // name_length
          0,                  // name_offset
          kExternalFunction,  // kind
          0                   // index
      });
      WasmExport* exp = &module_->export_table.back();

      exp->name_offset = consume_string(&exp->name_length, true, "field name");

      const byte* pos = pc();
      exp->kind = static_cast<WasmExternalKind>(consume_u8("export kind"));
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
        if (a.name_length != b.name_length) {
          return a.name_length < b.name_length;
        }
        const byte* left = start() + GetBufferRelativeOffset(a.name_offset);
        const byte* right = start() + GetBufferRelativeOffset(b.name_offset);
        return memcmp(left, right, a.name_length) < 0;
      };
      std::stable_sort(sorted_exports.begin(), sorted_exports.end(), cmp_less);

      auto it = sorted_exports.begin();
      WasmExport* last = &*it++;
      for (auto end = sorted_exports.end(); it != end; last = &*it++) {
        DCHECK(!cmp_less(*it, *last));  // Vector must be sorted.
        if (!cmp_less(*last, *it)) {
          const byte* pc = start() + GetBufferRelativeOffset(it->name_offset);
          errorf(pc, "Duplicate export name '%.*s' for functions %d and %d",
                 it->name_length, pc, last->index, it->index);
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
    for (uint32_t i = 0; ok() && i < element_count; ++i) {
      const byte* pos = pc();
      uint32_t table_index = consume_u32v("table index");
      if (table_index != 0) {
        errorf(pos, "illegal table index %u != 0", table_index);
      }
      WasmIndirectFunctionTable* table = nullptr;
      if (table_index >= module_->function_tables.size()) {
        errorf(pos, "out of bounds table index %u", table_index);
        break;
      }
      table = &module_->function_tables[table_index];
      WasmInitExpr offset = consume_init_expr(module_.get(), kWasmI32);
      uint32_t num_elem =
          consume_count("number of elements", kV8MaxWasmTableEntries);
      std::vector<uint32_t> vector;
      module_->table_inits.push_back({table_index, offset, vector});
      WasmTableInit* init = &module_->table_inits.back();
      for (uint32_t j = 0; j < num_elem; j++) {
        WasmFunction* func = nullptr;
        uint32_t index = consume_func_index(module_.get(), &func);
        DCHECK_IMPLIES(ok(), func != nullptr);
        if (!ok()) break;
        DCHECK_EQ(index, func->func_index);
        init->entries.push_back(index);
        // Canonicalize signature indices during decoding.
        table->map.FindOrInsert(func->sig);
      }
    }
  }

  void DecodeCodeSection(bool verify_functions) {
    const byte* pos = pc_;
    uint32_t functions_count = consume_u32v("functions count");
    if (functions_count != module_->num_declared_functions) {
      errorf(pos, "function body count %u mismatch (%u expected)",
             functions_count, module_->num_declared_functions);
    }
    for (uint32_t i = 0; ok() && i < functions_count; ++i) {
      WasmFunction* function =
          &module_->functions[i + module_->num_imported_functions];
      uint32_t size = consume_u32v("body size");
      uint32_t offset = pc_offset();
      consume_bytes(size, "function body");
      if (failed()) break;
      function->code_start_offset = offset;
      function->code_end_offset = offset + size;
      if (verify_functions) {
        ModuleBytesEnv module_env(module_.get(), nullptr,
                                  ModuleWireBytes(start_, end_));
        VerifyFunctionBody(module_->signature_zone->allocator(),
                           i + module_->num_imported_functions, &module_env,
                           function);
      }
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
          0,               // source_offset
          0                // source_size
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
      if (name_type == NameSectionType::kFunction) {
        uint32_t functions_count = inner.consume_u32v("functions count");

        for (; inner.ok() && functions_count > 0; --functions_count) {
          uint32_t function_index = inner.consume_u32v("function index");
          uint32_t name_length = 0;
          uint32_t name_offset =
              wasm::consume_string(inner, &name_length, false, "function name");

          // Be lenient with errors in the name section: Ignore illegal
          // or out-of-order indexes and non-UTF8 names. You can even assign
          // to the same function multiple times (last valid one wins).
          if (inner.ok() && function_index < module_->functions.size() &&
              unibrow::Utf8::Validate(
                  inner.start() + inner.GetBufferRelativeOffset(name_offset),
                  name_length)) {
            module_->functions[function_index].name_offset = name_offset;
            module_->functions[function_index].name_length = name_length;
          }
        }
      } else {
        inner.consume_bytes(name_payload_len, "name subsection payload");
      }
    }
    // Skip the whole names section in the outer decoder.
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
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
  FunctionResult DecodeSingleFunction(Zone* zone, ModuleBytesEnv* module_env,
                                      std::unique_ptr<WasmFunction> function) {
    pc_ = start_;
    function->sig = consume_sig(zone);       // read signature
    function->name_offset = 0;               // ---- name
    function->name_length = 0;               // ---- name length
    function->code_start_offset = off(pc_);  // ---- code start
    function->code_end_offset = off(end_);   // ---- code end

    if (ok())
      VerifyFunctionBody(zone->allocator(), 0, module_env, function.get());

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

 private:
  std::unique_ptr<WasmModule> module_;
  // The type section is the first section in a module.
  uint8_t next_section_ = kFirstSectionInModule;
  // We store next_section_ as uint8_t instead of SectionCode so that we can
  // increment it. This static_assert should make sure that SectionCode does not
  // get bigger than uint8_t accidentially.
  static_assert(sizeof(ModuleDecoder::next_section_) == sizeof(SectionCode),
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

  bool IsWithinLimit(uint32_t limit, uint32_t offset, uint32_t size) {
    if (offset > limit) return false;
    if ((offset + size) < offset) return false;  // overflow
    return (offset + size) <= limit;
  }

  // Decodes a single data segment entry inside a module starting at {pc_}.
  void DecodeDataSegmentInModule(WasmModule* module, WasmDataSegment* segment) {
    const byte* start = pc_;
    expect_u8("linear memory index", 0);
    segment->dest_addr = consume_init_expr(module, kWasmI32);
    segment->source_size = consume_u32v("source size");
    segment->source_offset = pc_offset();

    // Validate the data is in the decoder buffer.
    uint32_t limit = static_cast<uint32_t>(end_ - start_);
    if (!IsWithinLimit(limit, GetBufferRelativeOffset(segment->source_offset),
                       segment->source_size)) {
      error(start, "segment out of bounds of the section");
    }

    consume_bytes(segment->source_size, "segment data");
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
                          ModuleBytesEnv* menv, WasmFunction* function) {
    WasmFunctionName func_name(function,
                               menv->wire_bytes.GetNameOrNull(function));
    if (FLAG_trace_wasm_decoder || FLAG_trace_wasm_decode_time) {
      OFStream os(stdout);
      os << "Verifying WASM function " << func_name << std::endl;
    }
    FunctionBody body = {
        function->sig, start_,
        start_ + GetBufferRelativeOffset(function->code_start_offset),
        start_ + GetBufferRelativeOffset(function->code_end_offset)};
    DecodeResult result = VerifyWasmCode(
        allocator, menv == nullptr ? nullptr : menv->module_env.module, body);
    if (result.failed()) {
      // Wrap the error message from the function decoder.
      std::ostringstream str;
      str << "in function " << func_name << ": " << result.error_msg();

      // Set error code and location, if this is the first error.
      if (intermediate_result_.ok()) {
        intermediate_result_.MoveErrorFrom(result);
      }
    }
  }

  uint32_t consume_string(uint32_t* length, bool validate_utf8,
                          const char* name) {
    return wasm::consume_string(*this, length, validate_utf8, name);
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
                                uint32_t* maximum) {
    uint32_t flags = consume_u32v("resizable limits flags");
    const byte* pos = pc();
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
        GlobalIndexOperand<true> operand(this, pc() - 1);
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
        ImmI32Operand<true> operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kI32Const;
        expr.val.i32_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprF32Const: {
        ImmF32Operand<true> operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kF32Const;
        expr.val.f32_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprI64Const: {
        ImmI64Operand<true> operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kI64Const;
        expr.val.i64_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprF64Const: {
        ImmF64Operand<true> operand(this, pc() - 1);
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
        if (origin_ != kAsmJsOrigin && FLAG_wasm_simd_prototype) {
          switch (t) {
            case kLocalS128:
              return kWasmS128;
            case kLocalS1x4:
              return kWasmS1x4;
            case kLocalS1x8:
              return kWasmS1x8;
            case kLocalS1x16:
              return kWasmS1x16;
            default:
              break;
          }
        }
        error(pc_ - 1, "invalid local type");
        return kWasmStmt;
    }
  }

  // Parses a type entry, which is currently limited to functions only.
  FunctionSig* consume_sig(Zone* zone) {
    if (!expect_u8("type form", kWasmFunctionTypeForm)) return nullptr;
    // parse parameter types
    uint32_t param_count =
        consume_count("param count", kV8MaxWasmFunctionParams);
    if (failed()) return nullptr;
    std::vector<ValueType> params;
    for (uint32_t i = 0; ok() && i < param_count; ++i) {
      ValueType param = consume_value_type();
      params.push_back(param);
    }

    // parse return types
    const size_t max_return_count = FLAG_wasm_mv_prototype
                                        ? kV8MaxWasmFunctionMultiReturns
                                        : kV8MaxWasmFunctionReturns;
    uint32_t return_count = consume_count("return count", max_return_count);
    if (failed()) return nullptr;
    std::vector<ValueType> returns;
    for (uint32_t i = 0; ok() && i < return_count; ++i) {
      ValueType ret = consume_value_type();
      returns.push_back(ret);
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

ModuleResult DecodeWasmModuleInternal(Isolate* isolate,
                                      const byte* module_start,
                                      const byte* module_end,
                                      bool verify_functions,
                                      ModuleOrigin origin, bool is_sync) {
  size_t size = module_end - module_start;
  if (module_start > module_end) return ModuleResult::Error("start > end");
  if (size >= kV8MaxWasmModuleSize)
    return ModuleResult::Error("size > maximum module size: %zu", size);
  // TODO(bradnelson): Improve histogram handling of size_t.
  if (is_sync) {
    // TODO(karlschimpf): Make this work when asynchronous.
    // https://bugs.chromium.org/p/v8/issues/detail?id=6361
    (IsWasm(origin) ? isolate->counters()->wasm_wasm_module_size_bytes()
                    : isolate->counters()->wasm_asm_module_size_bytes())
        ->AddSample(static_cast<int>(size));
  }
  // Signatures are stored in zone memory, which have the same lifetime
  // as the {module}.
  ModuleDecoder decoder(module_start, module_end, origin);
  ModuleResult result = decoder.DecodeModule(isolate, verify_functions);
  // TODO(bradnelson): Improve histogram handling of size_t.
  // TODO(titzer): this isn't accurate, since it doesn't count the data
  // allocated on the C++ heap.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=657320
  if (is_sync && result.ok()) {
    // TODO(karlschimpf): Make this work when asynchronous.
    // https://bugs.chromium.org/p/v8/issues/detail?id=6361
    (IsWasm(origin)
         ? isolate->counters()->wasm_decode_wasm_module_peak_memory_bytes()
         : isolate->counters()->wasm_decode_asm_module_peak_memory_bytes())
        ->AddSample(
            static_cast<int>(result.val->signature_zone->allocation_size()));
  }
  return result;
}

}  // namespace

ModuleResult DecodeWasmModule(Isolate* isolate, const byte* module_start,
                              const byte* module_end, bool verify_functions,
                              ModuleOrigin origin, bool is_sync) {
  if (is_sync) {
    // TODO(karlschimpf): Make this work when asynchronous.
    // https://bugs.chromium.org/p/v8/issues/detail?id=6361
    HistogramTimerScope wasm_decode_module_time_scope(
        IsWasm(origin) ? isolate->counters()->wasm_decode_wasm_module_time()
                       : isolate->counters()->wasm_decode_asm_module_time());
    return DecodeWasmModuleInternal(isolate, module_start, module_end,
                                    verify_functions, origin, true);
  }
  return DecodeWasmModuleInternal(isolate, module_start, module_end,
                                  verify_functions, origin, false);
}

FunctionSig* DecodeWasmSignatureForTesting(Zone* zone, const byte* start,
                                           const byte* end) {
  ModuleDecoder decoder(start, end, kWasmOrigin);
  return decoder.DecodeFunctionSignature(zone, start);
}

WasmInitExpr DecodeWasmInitExprForTesting(const byte* start, const byte* end) {
  AccountingAllocator allocator;
  ModuleDecoder decoder(start, end, kWasmOrigin);
  return decoder.DecodeInitExpr(start);
}

namespace {

FunctionResult DecodeWasmFunctionInternal(Isolate* isolate, Zone* zone,
                                          ModuleBytesEnv* module_env,
                                          const byte* function_start,
                                          const byte* function_end,
                                          bool is_sync) {
  size_t size = function_end - function_start;
  if (function_start > function_end)
    return FunctionResult::Error("start > end");
  if (size > kV8MaxWasmFunctionSize)
    return FunctionResult::Error("size > maximum function size: %zu", size);
  if (is_sync) {
    // TODO(karlschimpf): Make this work when asynchronous.
    // https://bugs.chromium.org/p/v8/issues/detail?id=6361
    bool is_wasm = module_env->module_env.is_wasm();
    (is_wasm ? isolate->counters()->wasm_wasm_function_size_bytes()
             : isolate->counters()->wasm_asm_function_size_bytes())
        ->AddSample(static_cast<int>(size));
  }
  ModuleDecoder decoder(function_start, function_end, kWasmOrigin);
  return decoder.DecodeSingleFunction(
      zone, module_env, std::unique_ptr<WasmFunction>(new WasmFunction()));
}

}  // namespace

FunctionResult DecodeWasmFunction(Isolate* isolate, Zone* zone,
                                  ModuleBytesEnv* module_env,
                                  const byte* function_start,
                                  const byte* function_end, bool is_sync) {
  if (is_sync) {
    // TODO(karlschimpf): Make this work when asynchronous.
    // https://bugs.chromium.org/p/v8/issues/detail?id=6361
    size_t size = function_end - function_start;
    bool is_wasm = module_env->module_env.is_wasm();
    (is_wasm ? isolate->counters()->wasm_wasm_function_size_bytes()
             : isolate->counters()->wasm_asm_function_size_bytes())
        ->AddSample(static_cast<int>(size));
    HistogramTimerScope wasm_decode_function_time_scope(
        is_wasm ? isolate->counters()->wasm_decode_wasm_function_time()
                : isolate->counters()->wasm_decode_asm_function_time());
    return DecodeWasmFunctionInternal(isolate, zone, module_env, function_start,
                                      function_end, true);
  }
  return DecodeWasmFunctionInternal(isolate, zone, module_env, function_start,
                                    function_end, false);
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
    uint32_t payload_length = section_length - (payload_offset - section_start);
    decoder.consume_bytes(payload_length);
    result.push_back({section_start, name_offset, name_length, payload_offset,
                      payload_length, section_length});
  }

  return result;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
