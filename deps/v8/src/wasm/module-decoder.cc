// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-decoder.h"

#include "src/base/functional.h"
#include "src/base/platform/platform.h"
#include "src/flags.h"
#include "src/macro-assembler.h"
#include "src/objects.h"
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

  inline bool more() const {
    return section_code_ != kUnknownSectionCode && decoder_.more();
  }

  inline WasmSectionCode section_code() const { return section_code_; }

  inline const byte* section_start() const { return section_start_; }

  inline uint32_t section_length() const {
    return static_cast<uint32_t>(section_end_ - section_start_);
  }

  inline const byte* payload_start() const { return payload_start_; }

  inline uint32_t payload_length() const {
    return static_cast<uint32_t>(section_end_ - payload_start_);
  }

  inline const byte* section_end() const { return section_end_; }

  // Advances to the next section, checking that decoding the current section
  // stopped at {section_end_}.
  void advance() {
    if (decoder_.pc() != section_end_) {
      const char* msg = decoder_.pc() < section_end_ ? "shorter" : "longer";
      decoder_.error(decoder_.pc(), decoder_.pc(),
                     "section was %s than expected size "
                     "(%u bytes expected, %zu decoded)",
                     msg, section_length(),
                     static_cast<size_t>(decoder_.pc() - section_start_));
    }
    next();
  }

 private:
  Decoder& decoder_;
  WasmSectionCode section_code_;
  const byte* section_start_;
  const byte* payload_start_;
  const byte* section_end_;

  // Reads the section code/name at the current position and sets up
  // the internal fields.
  void next() {
    while (true) {
      if (!decoder_.more()) {
        section_code_ = kUnknownSectionCode;
        return;
      }
      uint8_t section_code = decoder_.consume_u8("section code");
      // Read and check the section size.
      uint32_t section_length = decoder_.consume_u32v("section length");
      section_start_ = decoder_.pc();
      payload_start_ = section_start_;
      if (decoder_.checkAvailable(section_length)) {
        // Get the limit of the section within the module.
        section_end_ = section_start_ + section_length;
      } else {
        // The section would extend beyond the end of the module.
        section_end_ = section_start_;
      }

      if (section_code == kUnknownSectionCode) {
        // Check for the known "name" section.
        uint32_t string_length = decoder_.consume_u32v("section name length");
        const byte* section_name_start = decoder_.pc();
        decoder_.consume_bytes(string_length, "section name");
        if (decoder_.failed() || decoder_.pc() > section_end_) {
          TRACE("Section name of length %u couldn't be read\n", string_length);
          section_code_ = kUnknownSectionCode;
          return;
        }
        payload_start_ = decoder_.pc();

        TRACE("  +%d  section name        : \"%.*s\"\n",
              static_cast<int>(section_name_start - decoder_.start()),
              string_length < 20 ? string_length : 20, section_name_start);

        if (string_length == kNameStringLength &&
            strncmp(reinterpret_cast<const char*>(section_name_start),
                    kNameString, kNameStringLength) == 0) {
          section_code = kNameSectionCode;
        } else {
          section_code = kUnknownSectionCode;
        }
      } else if (!IsValidSectionCode(section_code)) {
        decoder_.error(decoder_.pc(), decoder_.pc(),
                       "unknown section code #0x%02x", section_code);
        section_code = kUnknownSectionCode;
      }
      section_code_ = static_cast<WasmSectionCode>(section_code);

      TRACE("Section: %s\n", SectionName(section_code_));
      if (section_code_ == kUnknownSectionCode &&
          section_end_ > decoder_.pc()) {
        // skip to the end of the unknown section.
        uint32_t remaining =
            static_cast<uint32_t>(section_end_ - decoder_.pc());
        decoder_.consume_bytes(remaining, "section payload");
        // fall through and continue to the next section.
      } else {
        return;
      }
    }
  }
};

// The main logic for decoding the bytes of a module.
class ModuleDecoder : public Decoder {
 public:
  ModuleDecoder(Zone* zone, const byte* module_start, const byte* module_end,
                ModuleOrigin origin)
      : Decoder(module_start, module_end), module_zone(zone), origin_(origin) {
    result_.start = start_;
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
      fwrite(start_, end_ - start_, 1, wasm_file);
      fclose(wasm_file);
    }
  }

  // Decodes an entire module.
  ModuleResult DecodeModule(bool verify_functions = true) {
    pc_ = start_;
    WasmModule* module = new WasmModule(module_zone);
    module->min_mem_pages = 0;
    module->max_mem_pages = 0;
    module->mem_export = false;
    module->origin = origin_;

    const byte* pos = pc_;
    uint32_t magic_word = consume_u32("wasm magic");
#define BYTES(x) (x & 0xff), (x >> 8) & 0xff, (x >> 16) & 0xff, (x >> 24) & 0xff
    if (magic_word != kWasmMagic) {
      error(pos, pos,
            "expected magic word %02x %02x %02x %02x, "
            "found %02x %02x %02x %02x",
            BYTES(kWasmMagic), BYTES(magic_word));
    }

    pos = pc_;
    {
      uint32_t magic_version = consume_u32("wasm version");
      if (magic_version != kWasmVersion) {
        error(pos, pos,
              "expected version %02x %02x %02x %02x, "
              "found %02x %02x %02x %02x",
              BYTES(kWasmVersion), BYTES(magic_version));
      }
    }

    WasmSectionIterator section_iter(*this);

    // ===== Type section ====================================================
    if (section_iter.section_code() == kTypeSectionCode) {
      uint32_t signatures_count = consume_count("types count", kV8MaxWasmTypes);
      module->signatures.reserve(signatures_count);
      for (uint32_t i = 0; ok() && i < signatures_count; ++i) {
        TRACE("DecodeSignature[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));
        FunctionSig* s = consume_sig();
        module->signatures.push_back(s);
      }
      section_iter.advance();
    }

    // ===== Import section ==================================================
    if (section_iter.section_code() == kImportSectionCode) {
      uint32_t import_table_count =
          consume_count("imports count", kV8MaxWasmImports);
      module->import_table.reserve(import_table_count);
      for (uint32_t i = 0; ok() && i < import_table_count; ++i) {
        TRACE("DecodeImportTable[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));

        module->import_table.push_back({
            0,                  // module_name_length
            0,                  // module_name_offset
            0,                  // field_name_offset
            0,                  // field_name_length
            kExternalFunction,  // kind
            0                   // index
        });
        WasmImport* import = &module->import_table.back();
        const byte* pos = pc_;
        import->module_name_offset =
            consume_string(&import->module_name_length, true);
        import->field_name_offset =
            consume_string(&import->field_name_length, true);

        import->kind = static_cast<WasmExternalKind>(consume_u8("import kind"));
        switch (import->kind) {
          case kExternalFunction: {
            // ===== Imported function =======================================
            import->index = static_cast<uint32_t>(module->functions.size());
            module->num_imported_functions++;
            module->functions.push_back({nullptr,        // sig
                                         import->index,  // func_index
                                         0,              // sig_index
                                         0,              // name_offset
                                         0,              // name_length
                                         0,              // code_start_offset
                                         0,              // code_end_offset
                                         true,           // imported
                                         false});        // exported
            WasmFunction* function = &module->functions.back();
            function->sig_index = consume_sig_index(module, &function->sig);
            break;
          }
          case kExternalTable: {
            // ===== Imported table ==========================================
            if (!AddTable(module)) break;
            import->index =
                static_cast<uint32_t>(module->function_tables.size());
            module->function_tables.push_back({0, 0, false,
                                               std::vector<int32_t>(), true,
                                               false, SignatureMap()});
            expect_u8("element type", kWasmAnyFunctionTypeForm);
            WasmIndirectFunctionTable* table = &module->function_tables.back();
            consume_resizable_limits("element count", "elements",
                                     kV8MaxWasmTableSize, &table->min_size,
                                     &table->has_max, kV8MaxWasmTableSize,
                                     &table->max_size);
            break;
          }
          case kExternalMemory: {
            // ===== Imported memory =========================================
            if (!AddMemory(module)) break;
            consume_resizable_limits(
                "memory", "pages", kV8MaxWasmMemoryPages,
                &module->min_mem_pages, &module->has_max_mem,
                kSpecMaxWasmMemoryPages, &module->max_mem_pages);
            break;
          }
          case kExternalGlobal: {
            // ===== Imported global =========================================
            import->index = static_cast<uint32_t>(module->globals.size());
            module->globals.push_back(
                {kWasmStmt, false, WasmInitExpr(), 0, true, false});
            WasmGlobal* global = &module->globals.back();
            global->type = consume_value_type();
            global->mutability = consume_mutability();
            if (global->mutability) {
              error("mutable globals cannot be imported");
            }
            break;
          }
          default:
            error(pos, pos, "unknown import kind 0x%02x", import->kind);
            break;
        }
      }
      section_iter.advance();
    }

    // ===== Function section ================================================
    if (section_iter.section_code() == kFunctionSectionCode) {
      uint32_t functions_count =
          consume_count("functions count", kV8MaxWasmFunctions);
      module->functions.reserve(functions_count);
      module->num_declared_functions = functions_count;
      for (uint32_t i = 0; ok() && i < functions_count; ++i) {
        uint32_t func_index = static_cast<uint32_t>(module->functions.size());
        module->functions.push_back({nullptr,     // sig
                                     func_index,  // func_index
                                     0,           // sig_index
                                     0,           // name_offset
                                     0,           // name_length
                                     0,           // code_start_offset
                                     0,           // code_end_offset
                                     false,       // imported
                                     false});     // exported
        WasmFunction* function = &module->functions.back();
        function->sig_index = consume_sig_index(module, &function->sig);
      }
      section_iter.advance();
    }

    // ===== Table section ===================================================
    if (section_iter.section_code() == kTableSectionCode) {
      uint32_t table_count = consume_count("table count", kV8MaxWasmTables);

      for (uint32_t i = 0; ok() && i < table_count; i++) {
        if (!AddTable(module)) break;
        module->function_tables.push_back({0, 0, false, std::vector<int32_t>(),
                                           false, false, SignatureMap()});
        WasmIndirectFunctionTable* table = &module->function_tables.back();
        expect_u8("table type", kWasmAnyFunctionTypeForm);
        consume_resizable_limits(
            "table elements", "elements", kV8MaxWasmTableSize, &table->min_size,
            &table->has_max, kV8MaxWasmTableSize, &table->max_size);
      }
      section_iter.advance();
    }

    // ===== Memory section ==================================================
    if (section_iter.section_code() == kMemorySectionCode) {
      uint32_t memory_count = consume_count("memory count", kV8MaxWasmMemories);

      for (uint32_t i = 0; ok() && i < memory_count; i++) {
        if (!AddMemory(module)) break;
        consume_resizable_limits("memory", "pages", kV8MaxWasmMemoryPages,
                                 &module->min_mem_pages, &module->has_max_mem,
                                 kSpecMaxWasmMemoryPages,
                                 &module->max_mem_pages);
      }
      section_iter.advance();
    }

    // ===== Global section ==================================================
    if (section_iter.section_code() == kGlobalSectionCode) {
      uint32_t globals_count =
          consume_count("globals count", kV8MaxWasmGlobals);
      uint32_t imported_globals = static_cast<uint32_t>(module->globals.size());
      module->globals.reserve(imported_globals + globals_count);
      for (uint32_t i = 0; ok() && i < globals_count; ++i) {
        TRACE("DecodeGlobal[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));
        // Add an uninitialized global and pass a pointer to it.
        module->globals.push_back(
            {kWasmStmt, false, WasmInitExpr(), 0, false, false});
        WasmGlobal* global = &module->globals.back();
        DecodeGlobalInModule(module, i + imported_globals, global);
      }
      section_iter.advance();
    }

    // ===== Export section ==================================================
    if (section_iter.section_code() == kExportSectionCode) {
      uint32_t export_table_count =
          consume_count("exports count", kV8MaxWasmImports);
      module->export_table.reserve(export_table_count);
      for (uint32_t i = 0; ok() && i < export_table_count; ++i) {
        TRACE("DecodeExportTable[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));

        module->export_table.push_back({
            0,                  // name_length
            0,                  // name_offset
            kExternalFunction,  // kind
            0                   // index
        });
        WasmExport* exp = &module->export_table.back();

        exp->name_offset = consume_string(&exp->name_length, true);
        const byte* pos = pc();
        exp->kind = static_cast<WasmExternalKind>(consume_u8("export kind"));
        switch (exp->kind) {
          case kExternalFunction: {
            WasmFunction* func = nullptr;
            exp->index = consume_func_index(module, &func);
            module->num_exported_functions++;
            if (func) func->exported = true;
            break;
          }
          case kExternalTable: {
            WasmIndirectFunctionTable* table = nullptr;
            exp->index = consume_table_index(module, &table);
            if (table) table->exported = true;
            break;
          }
          case kExternalMemory: {
            uint32_t index = consume_u32v("memory index");
            // TODO(titzer): This should become more regular
            // once we support multiple memories.
            if (!module->has_memory || index != 0) {
              error("invalid memory index != 0");
            }
            module->mem_export = true;
            break;
          }
          case kExternalGlobal: {
            WasmGlobal* global = nullptr;
            exp->index = consume_global_index(module, &global);
            if (global) {
              if (global->mutability) {
                error("mutable globals cannot be exported");
              }
              global->exported = true;
            }
            break;
          }
          default:
            error(pos, pos, "invalid export kind 0x%02x", exp->kind);
            break;
        }
      }
      // Check for duplicate exports (except for asm.js).
      if (ok() && origin_ != kAsmJsOrigin && module->export_table.size() > 1) {
        std::vector<WasmExport> sorted_exports(module->export_table);
        const byte* base = start_;
        auto cmp_less = [base](const WasmExport& a, const WasmExport& b) {
          // Return true if a < b.
          if (a.name_length != b.name_length) {
            return a.name_length < b.name_length;
          }
          return memcmp(base + a.name_offset, base + b.name_offset,
                        a.name_length) < 0;
        };
        std::stable_sort(sorted_exports.begin(), sorted_exports.end(),
                         cmp_less);
        auto it = sorted_exports.begin();
        WasmExport* last = &*it++;
        for (auto end = sorted_exports.end(); it != end; last = &*it++) {
          DCHECK(!cmp_less(*it, *last));  // Vector must be sorted.
          if (!cmp_less(*last, *it)) {
            const byte* pc = start_ + it->name_offset;
            error(pc, pc,
                  "Duplicate export name '%.*s' for functions %d and %d",
                  it->name_length, pc, last->index, it->index);
            break;
          }
        }
      }
      section_iter.advance();
    }

    // ===== Start section ===================================================
    if (section_iter.section_code() == kStartSectionCode) {
      WasmFunction* func;
      const byte* pos = pc_;
      module->start_function_index = consume_func_index(module, &func);
      if (func &&
          (func->sig->parameter_count() > 0 || func->sig->return_count() > 0)) {
        error(pos,
              "invalid start function: non-zero parameter or return count");
      }
      section_iter.advance();
    }

    // ===== Elements section ================================================
    if (section_iter.section_code() == kElementSectionCode) {
      uint32_t element_count =
          consume_count("element count", kV8MaxWasmTableSize);
      for (uint32_t i = 0; ok() && i < element_count; ++i) {
        const byte* pos = pc();
        uint32_t table_index = consume_u32v("table index");
        if (table_index != 0) {
          error(pos, pos, "illegal table index %u != 0", table_index);
        }
        WasmIndirectFunctionTable* table = nullptr;
        if (table_index >= module->function_tables.size()) {
          error(pos, pos, "out of bounds table index %u", table_index);
        } else {
          table = &module->function_tables[table_index];
        }
        WasmInitExpr offset = consume_init_expr(module, kWasmI32);
        uint32_t num_elem =
            consume_count("number of elements", kV8MaxWasmTableEntries);
        std::vector<uint32_t> vector;
        module->table_inits.push_back({table_index, offset, vector});
        WasmTableInit* init = &module->table_inits.back();
        for (uint32_t j = 0; ok() && j < num_elem; j++) {
          WasmFunction* func = nullptr;
          uint32_t index = consume_func_index(module, &func);
          init->entries.push_back(index);
          if (table && index < module->functions.size()) {
            // Canonicalize signature indices during decoding.
            table->map.FindOrInsert(module->functions[index].sig);
          }
        }
      }

      section_iter.advance();
    }

    // ===== Code section ====================================================
    if (section_iter.section_code() == kCodeSectionCode) {
      const byte* pos = pc_;
      uint32_t functions_count = consume_u32v("functions count");
      if (functions_count != module->num_declared_functions) {
        error(pos, pos, "function body count %u mismatch (%u expected)",
              functions_count, module->num_declared_functions);
      }
      for (uint32_t i = 0; ok() && i < functions_count; ++i) {
        WasmFunction* function =
            &module->functions[i + module->num_imported_functions];
        uint32_t size = consume_u32v("body size");
        function->code_start_offset = pc_offset();
        function->code_end_offset = pc_offset() + size;
        if (verify_functions) {
          ModuleBytesEnv module_env(module, nullptr,
                                    ModuleWireBytes(start_, end_));
          VerifyFunctionBody(i + module->num_imported_functions, &module_env,
                             function);
        }
        consume_bytes(size, "function body");
      }
      section_iter.advance();
    }

    // ===== Data section ====================================================
    if (section_iter.section_code() == kDataSectionCode) {
      uint32_t data_segments_count =
          consume_count("data segments count", kV8MaxWasmDataSegments);
      module->data_segments.reserve(data_segments_count);
      for (uint32_t i = 0; ok() && i < data_segments_count; ++i) {
        if (!module->has_memory) {
          error("cannot load data without memory");
          break;
        }
        TRACE("DecodeDataSegment[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));
        module->data_segments.push_back({
            WasmInitExpr(),  // dest_addr
            0,               // source_offset
            0                // source_size
        });
        WasmDataSegment* segment = &module->data_segments.back();
        DecodeDataSegmentInModule(module, segment);
      }
      section_iter.advance();
    }

    // ===== Name section ====================================================
    if (section_iter.section_code() == kNameSectionCode) {
      // TODO(titzer): find a way to report name errors as warnings.
      // Use an inner decoder so that errors don't fail the outer decoder.
      Decoder inner(start_, pc_, end_);
      uint32_t functions_count = inner.consume_u32v("functions count");

      for (uint32_t i = 0; inner.ok() && i < functions_count; ++i) {
        uint32_t function_name_length = 0;
        uint32_t name_offset =
            consume_string(inner, &function_name_length, false);
        uint32_t func_index = i;
        if (inner.ok() && func_index < module->functions.size()) {
          module->functions[func_index].name_offset = name_offset;
          module->functions[func_index].name_length = function_name_length;
        }

        uint32_t local_names_count = inner.consume_u32v("local names count");
        for (uint32_t j = 0; ok() && j < local_names_count; j++) {
          uint32_t length = inner.consume_u32v("string length");
          inner.consume_bytes(length, "string");
        }
      }
      // Skip the whole names section in the outer decoder.
      consume_bytes(section_iter.payload_length(), nullptr);
      section_iter.advance();
    }

    // ===== Remaining sections ==============================================
    if (section_iter.more() && ok()) {
      error(pc(), pc(), "unexpected section: %s",
            SectionName(section_iter.section_code()));
    }

    if (ok()) {
      CalculateGlobalOffsets(module);
    }
    const WasmModule* finished_module = module;
    ModuleResult result = toResult(finished_module);
    if (verify_functions && result.ok()) {
      result.MoveFrom(result_);  // Copy error code and location.
    }
    if (FLAG_dump_wasm_module) DumpModule(result);
    return result;
  }

  // Decodes a single anonymous function starting at {start_}.
  FunctionResult DecodeSingleFunction(ModuleBytesEnv* module_env,
                                      WasmFunction* function) {
    pc_ = start_;
    function->sig = consume_sig();            // read signature
    function->name_offset = 0;                // ---- name
    function->name_length = 0;                // ---- name length
    function->code_start_offset = off(pc_);   // ---- code start
    function->code_end_offset = off(end_);    // ---- code end

    if (ok()) VerifyFunctionBody(0, module_env, function);

    FunctionResult result;
    result.MoveFrom(result_);  // Copy error code and location.
    result.val = function;
    return result;
  }

  // Decodes a single function signature at {start}.
  FunctionSig* DecodeFunctionSignature(const byte* start) {
    pc_ = start;
    FunctionSig* result = consume_sig();
    return ok() ? result : nullptr;
  }

  WasmInitExpr DecodeInitExpr(const byte* start) {
    pc_ = start;
    return consume_init_expr(nullptr, kWasmStmt);
  }

 private:
  Zone* module_zone;
  ModuleResult result_;
  ModuleOrigin origin_;

  uint32_t off(const byte* ptr) { return static_cast<uint32_t>(ptr - start_); }

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
          error(pos, pos,
                "invalid global index in init expression, "
                "index %u, other_index %u",
                index, other_index);
        } else if (module->globals[other_index].type != global->type) {
          error(pos, pos,
                "type mismatch in global initialization "
                "(from global #%u), expected %s, got %s",
                other_index, WasmOpcodes::TypeName(global->type),
                WasmOpcodes::TypeName(module->globals[other_index].type));
        }
        break;
      }
      default:
        if (global->type != TypeOf(module, global->init)) {
          error(pos, pos,
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
    segment->source_offset = static_cast<uint32_t>(pc_ - start_);

    // Validate the data is in the module.
    uint32_t module_limit = static_cast<uint32_t>(end_ - start_);
    if (!IsWithinLimit(module_limit, segment->source_offset,
                       segment->source_size)) {
      error(start, "segment out of bounds of module");
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
  void VerifyFunctionBody(uint32_t func_num, ModuleBytesEnv* menv,
                          WasmFunction* function) {
    if (FLAG_trace_wasm_decoder || FLAG_trace_wasm_decode_time) {
      OFStream os(stdout);
      os << "Verifying WASM function " << WasmFunctionName(function, menv)
         << std::endl;
    }
    FunctionBody body = {function->sig, start_,
                         start_ + function->code_start_offset,
                         start_ + function->code_end_offset};
    DecodeResult result =
        VerifyWasmCode(module_zone->allocator(),
                       menv == nullptr ? nullptr : menv->module, body);
    if (result.failed()) {
      // Wrap the error message from the function decoder.
      std::ostringstream str;
      str << "in function " << WasmFunctionName(function, menv) << ": ";
      str << result;
      std::string strval = str.str();
      const char* raw = strval.c_str();
      size_t len = strlen(raw);
      char* buffer = new char[len];
      strncpy(buffer, raw, len);
      buffer[len - 1] = 0;

      // Copy error code and location.
      result_.MoveFrom(result);
      result_.error_msg.reset(buffer);
    }
  }

  uint32_t consume_string(uint32_t* length, bool validate_utf8) {
    return consume_string(*this, length, validate_utf8);
  }

  // Reads a length-prefixed string, checking that it is within bounds. Returns
  // the offset of the string, and the length as an out parameter.
  uint32_t consume_string(Decoder& decoder, uint32_t* length,
                          bool validate_utf8) {
    *length = decoder.consume_u32v("string length");
    uint32_t offset = decoder.pc_offset();
    const byte* string_start = decoder.pc();
    // Consume bytes before validation to guarantee that the string is not oob.
    if (*length > 0) decoder.consume_bytes(*length, "string");
    if (decoder.ok() && validate_utf8 &&
        !unibrow::Utf8::Validate(string_start, *length)) {
      decoder.error(string_start, "no valid UTF-8 string");
    }
    return offset;
  }

  uint32_t consume_sig_index(WasmModule* module, FunctionSig** sig) {
    const byte* pos = pc_;
    uint32_t sig_index = consume_u32v("signature index");
    if (sig_index >= module->signatures.size()) {
      error(pos, pos, "signature index %u out of bounds (%d signatures)",
            sig_index, static_cast<int>(module->signatures.size()));
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
      error(p, p, "%s of %u exceeds internal limit of %zu", name, count,
            maximum);
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
      error(pos, pos, "%s %u out of bounds (%d entries)", name, index,
            static_cast<int>(vector.size()));
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
      error(pos, pos,
            "initial %s size (%u %s) is larger than implementation limit (%u)",
            name, *initial, units, max_initial);
    }
    if (flags & 1) {
      *has_max = true;
      pos = pc();
      *maximum = consume_u32v("maximum size");
      if (*maximum > max_maximum) {
        error(
            pos, pos,
            "maximum %s size (%u %s) is larger than implementation limit (%u)",
            name, *maximum, units, max_maximum);
      }
      if (*maximum < *initial) {
        error(pos, pos, "maximum %s size (%u %s) is less than initial (%u %s)",
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
      error(pos, pos, "expected %s 0x%02x, got 0x%02x", name, expected, value);
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
        GlobalIndexOperand operand(this, pc() - 1);
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
        ImmI32Operand operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kI32Const;
        expr.val.i32_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprF32Const: {
        ImmF32Operand operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kF32Const;
        expr.val.f32_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprI64Const: {
        ImmI64Operand operand(this, pc() - 1);
        expr.kind = WasmInitExpr::kI64Const;
        expr.val.i64_const = operand.value;
        len = operand.length;
        break;
      }
      case kExprF64Const: {
        ImmF64Operand operand(this, pc() - 1);
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
      error(pos, pos, "type error in init expression, expected %s, got %s",
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
      case kLocalS128:
        if (origin_ != kAsmJsOrigin && FLAG_wasm_simd_prototype) {
          return kWasmS128;
        } else {
          error(pc_ - 1, "invalid local type");
          return kWasmStmt;
        }
      default:
        error(pc_ - 1, "invalid local type");
        return kWasmStmt;
    }
  }

  // Parses a type entry, which is currently limited to functions only.
  FunctionSig* consume_sig() {
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
    ValueType* buffer =
        module_zone->NewArray<ValueType>(param_count + return_count);
    uint32_t b = 0;
    for (uint32_t i = 0; i < return_count; ++i) buffer[b++] = returns[i];
    for (uint32_t i = 0; i < param_count; ++i) buffer[b++] = params[i];

    return new (module_zone) FunctionSig(return_count, param_count, buffer);
  }
};

// Helpers for nice error messages.
class ModuleError : public ModuleResult {
 public:
  explicit ModuleError(const char* msg) {
    error_code = kError;
    size_t len = strlen(msg) + 1;
    char* result = new char[len];
    strncpy(result, msg, len);
    result[len - 1] = 0;
    error_msg.reset(result);
  }
};

// Helpers for nice error messages.
class FunctionError : public FunctionResult {
 public:
  explicit FunctionError(const char* msg) {
    error_code = kError;
    size_t len = strlen(msg) + 1;
    char* result = new char[len];
    strncpy(result, msg, len);
    result[len - 1] = 0;
    error_msg.reset(result);
  }
};

// Find section with given section code. Return Vector of the payload, or null
// Vector if section is not found or module bytes are invalid.
Vector<const byte> FindSection(const byte* module_start, const byte* module_end,
                               WasmSectionCode code) {
  Decoder decoder(module_start, module_end);

  uint32_t magic_word = decoder.consume_u32("wasm magic");
  if (magic_word != kWasmMagic) decoder.error("wrong magic word");

  uint32_t magic_version = decoder.consume_u32("wasm version");
  if (magic_version != kWasmVersion) decoder.error("wrong wasm version");

  WasmSectionIterator section_iter(decoder);
  while (section_iter.more()) {
    if (section_iter.section_code() == code) {
      return Vector<const uint8_t>(section_iter.payload_start(),
                                   section_iter.payload_length());
    }
    decoder.consume_bytes(section_iter.payload_length(), "section payload");
    section_iter.advance();
  }

  return Vector<const uint8_t>();
}

}  // namespace

ModuleResult DecodeWasmModule(Isolate* isolate, const byte* module_start,
                              const byte* module_end, bool verify_functions,
                              ModuleOrigin origin) {
  HistogramTimerScope wasm_decode_module_time_scope(
      isolate->counters()->wasm_decode_module_time());
  size_t size = module_end - module_start;
  if (module_start > module_end) return ModuleError("start > end");
  if (size >= kV8MaxWasmModuleSize)
    return ModuleError("size > maximum module size");
  // TODO(bradnelson): Improve histogram handling of size_t.
  isolate->counters()->wasm_module_size_bytes()->AddSample(
      static_cast<int>(size));
  // Signatures are stored in zone memory, which have the same lifetime
  // as the {module}.
  Zone* zone = new Zone(isolate->allocator(), ZONE_NAME);
  ModuleDecoder decoder(zone, module_start, module_end, origin);
  ModuleResult result = decoder.DecodeModule(verify_functions);
  // TODO(bradnelson): Improve histogram handling of size_t.
  // TODO(titzer): this isn't accurate, since it doesn't count the data
  // allocated on the C++ heap.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=657320
  isolate->counters()->wasm_decode_module_peak_memory_bytes()->AddSample(
      static_cast<int>(zone->allocation_size()));
  return result;
}

FunctionSig* DecodeWasmSignatureForTesting(Zone* zone, const byte* start,
                                           const byte* end) {
  ModuleDecoder decoder(zone, start, end, kWasmOrigin);
  return decoder.DecodeFunctionSignature(start);
}

WasmInitExpr DecodeWasmInitExprForTesting(const byte* start, const byte* end) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  ModuleDecoder decoder(&zone, start, end, kWasmOrigin);
  return decoder.DecodeInitExpr(start);
}

FunctionResult DecodeWasmFunction(Isolate* isolate, Zone* zone,
                                  ModuleBytesEnv* module_env,
                                  const byte* function_start,
                                  const byte* function_end) {
  HistogramTimerScope wasm_decode_function_time_scope(
      isolate->counters()->wasm_decode_function_time());
  size_t size = function_end - function_start;
  if (function_start > function_end) return FunctionError("start > end");
  if (size > kV8MaxWasmFunctionSize)
    return FunctionError("size > maximum function size");
  isolate->counters()->wasm_function_size_bytes()->AddSample(
      static_cast<int>(size));
  WasmFunction* function = new WasmFunction();
  ModuleDecoder decoder(zone, function_start, function_end, kWasmOrigin);
  return decoder.DecodeSingleFunction(module_env, function);
}

FunctionOffsetsResult DecodeWasmFunctionOffsets(const byte* module_start,
                                                const byte* module_end) {
  // Find and decode the code section.
  Vector<const byte> code_section =
      FindSection(module_start, module_end, kCodeSectionCode);
  Decoder decoder(code_section.start(), code_section.end());
  FunctionOffsets table;
  if (!code_section.start()) {
    decoder.error("no code section");
    return decoder.toResult(std::move(table));
  }

  uint32_t functions_count = decoder.consume_u32v("functions count");
  // Reserve space for the entries, taking care of invalid input.
  if (functions_count < static_cast<unsigned>(code_section.length()) / 2) {
    table.reserve(functions_count);
  }

  int section_offset = static_cast<int>(code_section.start() - module_start);
  DCHECK_LE(0, section_offset);
  for (uint32_t i = 0; i < functions_count && decoder.ok(); ++i) {
    uint32_t size = decoder.consume_u32v("body size");
    int offset = static_cast<int>(section_offset + decoder.pc_offset());
    table.push_back(std::make_pair(offset, static_cast<int>(size)));
    DCHECK(table.back().first >= 0 && table.back().second >= 0);
    decoder.consume_bytes(size);
  }
  if (decoder.more()) decoder.error("unexpected additional bytes");

  return decoder.toResult(std::move(table));
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
      table.push_back(std::vector<AsmJsOffsetEntry>());
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
