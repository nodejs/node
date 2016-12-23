// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-decoder.h"

#include "src/base/functional.h"
#include "src/base/platform/platform.h"
#include "src/macro-assembler.h"
#include "src/objects.h"
#include "src/v8.h"

#include "src/wasm/decoder.h"

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

LocalType TypeOf(const WasmModule* module, const WasmInitExpr& expr) {
  switch (expr.kind) {
    case WasmInitExpr::kNone:
      return kAstStmt;
    case WasmInitExpr::kGlobalIndex:
      return expr.val.global_index < module->globals.size()
                 ? module->globals[expr.val.global_index].type
                 : kAstStmt;
    case WasmInitExpr::kI32Const:
      return kAstI32;
    case WasmInitExpr::kI64Const:
      return kAstI64;
    case WasmInitExpr::kF32Const:
      return kAstF32;
    case WasmInitExpr::kF64Const:
      return kAstF64;
    default:
      UNREACHABLE();
      return kAstStmt;
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
      if (decoder_.checkAvailable(section_length)) {
        // Get the limit of the section within the module.
        section_end_ = section_start_ + section_length;
      } else {
        // The section would extend beyond the end of the module.
        section_end_ = section_start_;
      }

      if (section_code == kUnknownSectionCode) {
        // Check for the known "names" section.
        uint32_t string_length = decoder_.consume_u32v("section name length");
        const byte* section_name_start = decoder_.pc();
        decoder_.consume_bytes(string_length, "section name");
        if (decoder_.failed() || decoder_.pc() > section_end_) {
          TRACE("Section name of length %u couldn't be read\n", string_length);
          section_code_ = kUnknownSectionCode;
          return;
        }

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
    if (limit_ < start_) {
      error(start_, "end is less than start");
      limit_ = start_;
    }
  }

  virtual void onFirstError() {
    pc_ = limit_;  // On error, terminate section decoding loop.
  }

  static void DumpModule(WasmModule* module, const ModuleResult& result) {
    std::string path;
    if (FLAG_dump_wasm_module_path) {
      path = FLAG_dump_wasm_module_path;
      if (path.size() &&
          !base::OS::isDirectorySeparator(path[path.size() - 1])) {
        path += base::OS::DirectorySeparator();
      }
    }
    // File are named `HASH.{ok,failed}.wasm`.
    size_t hash = base::hash_range(module->module_start, module->module_end);
    char buf[32] = {'\0'};
#if V8_OS_WIN && _MSC_VER < 1900
#define snprintf sprintf_s
#endif
    snprintf(buf, sizeof(buf) - 1, "%016zx.%s.wasm", hash,
             result.ok() ? "ok" : "failed");
    std::string name(buf);
    if (FILE* wasm_file = base::OS::FOpen((path + name).c_str(), "wb")) {
      fwrite(module->module_start, module->module_end - module->module_start, 1,
             wasm_file);
      fclose(wasm_file);
    }
  }

  // Decodes an entire module.
  ModuleResult DecodeModule(WasmModule* module, bool verify_functions = true) {
    pc_ = start_;
    module->module_start = start_;
    module->module_end = limit_;
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
      uint32_t signatures_count = consume_u32v("signatures count");
      module->signatures.reserve(SafeReserve(signatures_count));
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
      uint32_t import_table_count = consume_u32v("import table count");
      module->import_table.reserve(SafeReserve(import_table_count));
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
        if (import->module_name_length == 0) {
          error(pos, "import module name cannot be NULL");
        }
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
            import->index =
                static_cast<uint32_t>(module->function_tables.size());
            module->function_tables.push_back(
                {0, 0, std::vector<int32_t>(), true, false});
            expect_u8("element type", 0x20);
            WasmIndirectFunctionTable* table = &module->function_tables.back();
            consume_resizable_limits("element count", "elements", kMaxUInt32,
                                     &table->size, &table->max_size);
            break;
          }
          case kExternalMemory: {
            // ===== Imported memory =========================================
            //            import->index =
            //            static_cast<uint32_t>(module->memories.size());
            // TODO(titzer): imported memories
            break;
          }
          case kExternalGlobal: {
            // ===== Imported global =========================================
            import->index = static_cast<uint32_t>(module->globals.size());
            module->globals.push_back(
                {kAstStmt, false, NO_INIT, 0, true, false});
            WasmGlobal* global = &module->globals.back();
            global->type = consume_value_type();
            global->mutability = consume_u8("mutability") != 0;
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
      uint32_t functions_count = consume_u32v("functions count");
      module->functions.reserve(SafeReserve(functions_count));
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
      const byte* pos = pc_;
      uint32_t table_count = consume_u32v("table count");
      // Require at most one table for now.
      if (table_count > 1) {
        error(pos, pos, "invalid table count %d, maximum 1", table_count);
      }

      for (uint32_t i = 0; ok() && i < table_count; i++) {
        module->function_tables.push_back(
            {0, 0, std::vector<int32_t>(), false, false});
        WasmIndirectFunctionTable* table = &module->function_tables.back();
        expect_u8("table type", kWasmAnyFunctionTypeForm);
        consume_resizable_limits("table elements", "elements", kMaxUInt32,
                                 &table->size, &table->max_size);
      }
      section_iter.advance();
    }

    // ===== Memory section ==================================================
    if (section_iter.section_code() == kMemorySectionCode) {
      const byte* pos = pc_;
      uint32_t memory_count = consume_u32v("memory count");
      // Require at most one memory for now.
      if (memory_count > 1) {
        error(pos, pos, "invalid memory count %d, maximum 1", memory_count);
      }

      for (uint32_t i = 0; ok() && i < memory_count; i++) {
        consume_resizable_limits("memory", "pages", WasmModule::kMaxLegalPages,
                                 &module->min_mem_pages,
                                 &module->max_mem_pages);
      }
      section_iter.advance();
    }

    // ===== Global section ==================================================
    if (section_iter.section_code() == kGlobalSectionCode) {
      uint32_t globals_count = consume_u32v("globals count");
      module->globals.reserve(SafeReserve(globals_count));
      for (uint32_t i = 0; ok() && i < globals_count; ++i) {
        TRACE("DecodeGlobal[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));
        // Add an uninitialized global and pass a pointer to it.
        module->globals.push_back({kAstStmt, false, NO_INIT, 0, false, false});
        WasmGlobal* global = &module->globals.back();
        DecodeGlobalInModule(module, i, global);
      }
      section_iter.advance();
    }

    // ===== Export section ==================================================
    if (section_iter.section_code() == kExportSectionCode) {
      uint32_t export_table_count = consume_u32v("export table count");
      module->export_table.reserve(SafeReserve(export_table_count));
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
            if (index != 0) error("invalid memory index != 0");
            module->mem_export = true;
            break;
          }
          case kExternalGlobal: {
            WasmGlobal* global = nullptr;
            exp->index = consume_global_index(module, &global);
            if (global) global->exported = true;
            break;
          }
          default:
            error(pos, pos, "invalid export kind 0x%02x", exp->kind);
            break;
        }
      }
      // Check for duplicate exports.
      if (ok() && module->export_table.size() > 1) {
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
      if (func && func->sig->parameter_count() > 0) {
        error(pos, "invalid start function: non-zero parameter count");
      }
      section_iter.advance();
    }

    // ===== Elements section ================================================
    if (section_iter.section_code() == kElementSectionCode) {
      uint32_t element_count = consume_u32v("element count");
      for (uint32_t i = 0; ok() && i < element_count; ++i) {
        uint32_t table_index = consume_u32v("table index");
        if (table_index != 0) error("illegal table index != 0");
        WasmInitExpr offset = consume_init_expr(module, kAstI32);
        uint32_t num_elem = consume_u32v("number of elements");
        std::vector<uint32_t> vector;
        module->table_inits.push_back({table_index, offset, vector});
        WasmTableInit* init = &module->table_inits.back();
        init->entries.reserve(SafeReserve(num_elem));
        for (uint32_t j = 0; ok() && j < num_elem; j++) {
          WasmFunction* func = nullptr;
          init->entries.push_back(consume_func_index(module, &func));
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
        consume_bytes(size, "function body");
      }
      section_iter.advance();
    }

    // ===== Data section ====================================================
    if (section_iter.section_code() == kDataSectionCode) {
      uint32_t data_segments_count = consume_u32v("data segments count");
      module->data_segments.reserve(SafeReserve(data_segments_count));
      for (uint32_t i = 0; ok() && i < data_segments_count; ++i) {
        TRACE("DecodeDataSegment[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));
        module->data_segments.push_back({
            NO_INIT,  // dest_addr
            0,        // source_offset
            0         // source_size
        });
        WasmDataSegment* segment = &module->data_segments.back();
        DecodeDataSegmentInModule(module, segment);
      }
      section_iter.advance();
    }

    // ===== Name section ====================================================
    if (section_iter.section_code() == kNameSectionCode) {
      const byte* pos = pc_;
      uint32_t functions_count = consume_u32v("functions count");
      if (functions_count != module->num_declared_functions) {
        error(pos, pos, "function name count %u mismatch (%u expected)",
              functions_count, module->num_declared_functions);
      }

      for (uint32_t i = 0; ok() && i < functions_count; ++i) {
        WasmFunction* function =
            &module->functions[i + module->num_imported_functions];
        function->name_offset = consume_string(&function->name_length, false);

        uint32_t local_names_count = consume_u32v("local names count");
        for (uint32_t j = 0; ok() && j < local_names_count; j++) {
          uint32_t unused = 0;
          uint32_t offset = consume_string(&unused, false);
          USE(unused);
          USE(offset);
        }
      }
      section_iter.advance();
    }

    // ===== Remaining sections ==============================================
    if (section_iter.more() && ok()) {
      error(pc(), pc(), "unexpected section: %s",
            SectionName(section_iter.section_code()));
    }

    if (ok()) {
      CalculateGlobalOffsets(module);
      PreinitializeIndirectFunctionTables(module);
    }
    const WasmModule* finished_module = module;
    ModuleResult result = toResult(finished_module);
    if (FLAG_dump_wasm_module) DumpModule(module, result);
    return result;
  }

  uint32_t SafeReserve(uint32_t count) {
    // Avoid OOM by only reserving up to a certain size.
    const uint32_t kMaxReserve = 20000;
    return count < kMaxReserve ? count : kMaxReserve;
  }

  // Decodes a single anonymous function starting at {start_}.
  FunctionResult DecodeSingleFunction(ModuleEnv* module_env,
                                      WasmFunction* function) {
    pc_ = start_;
    function->sig = consume_sig();            // read signature
    function->name_offset = 0;                // ---- name
    function->name_length = 0;                // ---- name length
    function->code_start_offset = off(pc_);   // ---- code start
    function->code_end_offset = off(limit_);  // ---- code end

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
    return consume_init_expr(nullptr, kAstStmt);
  }

 private:
  Zone* module_zone;
  ModuleResult result_;
  ModuleOrigin origin_;

  uint32_t off(const byte* ptr) { return static_cast<uint32_t>(ptr - start_); }

  // Decodes a single global entry inside a module starting at {pc_}.
  void DecodeGlobalInModule(WasmModule* module, uint32_t index,
                            WasmGlobal* global) {
    global->type = consume_value_type();
    global->mutability = consume_u8("mutability") != 0;
    const byte* pos = pc();
    global->init = consume_init_expr(module, kAstStmt);
    switch (global->init.kind) {
      case WasmInitExpr::kGlobalIndex:
        if (global->init.val.global_index >= index) {
          error("invalid global index in init expression");
        } else if (module->globals[index].type != global->type) {
          error("type mismatch in global initialization");
        }
        break;
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
    segment->dest_addr = consume_init_expr(module, kAstI32);
    segment->source_size = consume_u32v("source size");
    segment->source_offset = static_cast<uint32_t>(pc_ - start_);

    // Validate the data is in the module.
    uint32_t module_limit = static_cast<uint32_t>(limit_ - start_);
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

  // TODO(titzer): this only works without overlapping initializations from
  // global bases for entries
  void PreinitializeIndirectFunctionTables(WasmModule* module) {
    // Fill all tables with invalid entries first.
    for (WasmIndirectFunctionTable& table : module->function_tables) {
      table.values.resize(table.size);
      for (size_t i = 0; i < table.size; i++) {
        table.values[i] = kInvalidFunctionIndex;
      }
    }
    for (WasmTableInit& init : module->table_inits) {
      if (init.offset.kind != WasmInitExpr::kI32Const) continue;
      if (init.table_index >= module->function_tables.size()) continue;
      WasmIndirectFunctionTable& table =
          module->function_tables[init.table_index];
      for (size_t i = 0; i < init.entries.size(); i++) {
        size_t index = i + init.offset.val.i32_const;
        if (index < table.values.size()) {
          table.values[index] = init.entries[i];
        }
      }
    }
  }

  // Verifies the body (code) of a given function.
  void VerifyFunctionBody(uint32_t func_num, ModuleEnv* menv,
                          WasmFunction* function) {
    if (FLAG_trace_wasm_decoder || FLAG_trace_wasm_decode_time) {
      OFStream os(stdout);
      os << "Verifying WASM function " << WasmFunctionName(function, menv)
         << std::endl;
    }
    FunctionBody body = {menv, function->sig, start_,
                         start_ + function->code_start_offset,
                         start_ + function->code_end_offset};
    DecodeResult result = VerifyWasmCode(module_zone->allocator(), body);
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

  // Reads a length-prefixed string, checking that it is within bounds. Returns
  // the offset of the string, and the length as an out parameter.
  uint32_t consume_string(uint32_t* length, bool validate_utf8) {
    *length = consume_u32v("string length");
    uint32_t offset = pc_offset();
    const byte* string_start = pc_;
    // Consume bytes before validation to guarantee that the string is not oob.
    consume_bytes(*length, "string");
    if (ok() && validate_utf8 &&
        !unibrow::Utf8::Validate(string_start, *length)) {
      error(string_start, "no valid UTF-8 string");
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
                                uint32_t max_value, uint32_t* initial,
                                uint32_t* maximum) {
    uint32_t flags = consume_u32v("resizable limits flags");
    const byte* pos = pc();
    *initial = consume_u32v("initial size");
    if (*initial > max_value) {
      error(pos, pos,
            "initial %s size (%u %s) is larger than maximum allowable (%u)",
            name, *initial, units, max_value);
    }
    if (flags & 1) {
      pos = pc();
      *maximum = consume_u32v("maximum size");
      if (*maximum > max_value) {
        error(pos, pos,
              "maximum %s size (%u %s) is larger than maximum allowable (%u)",
              name, *maximum, units, max_value);
      }
      if (*maximum < *initial) {
        error(pos, pos, "maximum %s size (%u %s) is less than initial (%u %s)",
              name, *maximum, units, *initial, units);
      }
    } else {
      *maximum = 0;
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

  WasmInitExpr consume_init_expr(WasmModule* module, LocalType expected) {
    const byte* pos = pc();
    uint8_t opcode = consume_u8("opcode");
    WasmInitExpr expr;
    unsigned len = 0;
    switch (opcode) {
      case kExprGetGlobal: {
        GlobalIndexOperand operand(this, pc() - 1);
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
    if (expected != kAstStmt && TypeOf(module, expr) != kAstI32) {
      error(pos, pos, "type error in init expression, expected %s, got %s",
            WasmOpcodes::TypeName(expected),
            WasmOpcodes::TypeName(TypeOf(module, expr)));
    }
    return expr;
  }

  // Reads a single 8-bit integer, interpreting it as a local type.
  LocalType consume_value_type() {
    byte val = consume_u8("value type");
    LocalTypeCode t = static_cast<LocalTypeCode>(val);
    switch (t) {
      case kLocalI32:
        return kAstI32;
      case kLocalI64:
        return kAstI64;
      case kLocalF32:
        return kAstF32;
      case kLocalF64:
        return kAstF64;
      case kLocalS128:
        return kAstS128;
      default:
        error(pc_ - 1, "invalid local type");
        return kAstStmt;
    }
  }

  // Parses a type entry, which is currently limited to functions only.
  FunctionSig* consume_sig() {
    if (!expect_u8("type form", kWasmFunctionTypeForm)) return nullptr;
    // parse parameter types
    uint32_t param_count = consume_u32v("param count");
    std::vector<LocalType> params;
    for (uint32_t i = 0; ok() && i < param_count; ++i) {
      LocalType param = consume_value_type();
      params.push_back(param);
    }

    // parse return types
    const byte* pt = pc_;
    uint32_t return_count = consume_u32v("return count");
    if (return_count > kMaxReturnCount) {
      error(pt, pt, "return count of %u exceeds maximum of %u", return_count,
            kMaxReturnCount);
      return nullptr;
    }
    std::vector<LocalType> returns;
    for (uint32_t i = 0; ok() && i < return_count; ++i) {
      LocalType ret = consume_value_type();
      returns.push_back(ret);
    }

    if (failed()) {
      // Decoding failed, return void -> void
      return new (module_zone) FunctionSig(0, 0, nullptr);
    }

    // FunctionSig stores the return types first.
    LocalType* buffer =
        module_zone->NewArray<LocalType>(param_count + return_count);
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
      return Vector<const uint8_t>(section_iter.section_start(),
                                   section_iter.section_length());
    }
    decoder.consume_bytes(section_iter.section_length(), "section payload");
    section_iter.advance();
  }

  return Vector<const uint8_t>();
}

}  // namespace

ModuleResult DecodeWasmModule(Isolate* isolate, Zone* zone,
                              const byte* module_start, const byte* module_end,
                              bool verify_functions, ModuleOrigin origin) {
  size_t decode_memory_start = zone->allocation_size();
  HistogramTimerScope wasm_decode_module_time_scope(
      isolate->counters()->wasm_decode_module_time());
  size_t size = module_end - module_start;
  if (module_start > module_end) return ModuleError("start > end");
  if (size >= kMaxModuleSize) return ModuleError("size > maximum module size");
  // TODO(bradnelson): Improve histogram handling of size_t.
  isolate->counters()->wasm_module_size_bytes()->AddSample(
      static_cast<int>(size));
  WasmModule* module = new WasmModule();
  ModuleDecoder decoder(zone, module_start, module_end, origin);
  ModuleResult result = decoder.DecodeModule(module, verify_functions);
  // TODO(bradnelson): Improve histogram handling of size_t.
  isolate->counters()->wasm_decode_module_peak_memory_bytes()->AddSample(
      static_cast<int>(zone->allocation_size() - decode_memory_start));
  return result;
}

FunctionSig* DecodeWasmSignatureForTesting(Zone* zone, const byte* start,
                                           const byte* end) {
  ModuleDecoder decoder(zone, start, end, kWasmOrigin);
  return decoder.DecodeFunctionSignature(start);
}

WasmInitExpr DecodeWasmInitExprForTesting(const byte* start, const byte* end) {
  AccountingAllocator allocator;
  Zone zone(&allocator);
  ModuleDecoder decoder(&zone, start, end, kWasmOrigin);
  return decoder.DecodeInitExpr(start);
}

FunctionResult DecodeWasmFunction(Isolate* isolate, Zone* zone,
                                  ModuleEnv* module_env,
                                  const byte* function_start,
                                  const byte* function_end) {
  HistogramTimerScope wasm_decode_function_time_scope(
      isolate->counters()->wasm_decode_function_time());
  size_t size = function_end - function_start;
  if (function_start > function_end) return FunctionError("start > end");
  if (size > kMaxFunctionSize)
    return FunctionError("size > maximum function size");
  isolate->counters()->wasm_function_size_bytes()->AddSample(
      static_cast<int>(size));
  WasmFunction* function = new WasmFunction();
  ModuleDecoder decoder(zone, function_start, function_end, kWasmOrigin);
  return decoder.DecodeSingleFunction(module_env, function);
}

FunctionOffsetsResult DecodeWasmFunctionOffsets(
    const byte* module_start, const byte* module_end,
    uint32_t num_imported_functions) {
  // Find and decode the code section.
  Vector<const byte> code_section =
      FindSection(module_start, module_end, kCodeSectionCode);
  Decoder decoder(code_section.start(), code_section.end());
  FunctionOffsets table;
  if (!code_section.start()) {
    decoder.error("no code section");
    return decoder.toResult(std::move(table));
  }

  // Reserve entries for the imported functions.
  table.reserve(num_imported_functions);
  for (uint32_t i = 0; i < num_imported_functions; i++) {
    table.push_back(std::make_pair(0, 0));
  }

  uint32_t functions_count = decoder.consume_u32v("functions count");
  // Take care of invalid input here.
  if (functions_count < static_cast<unsigned>(code_section.length()) / 2)
    table.reserve(functions_count);
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

}  // namespace wasm
}  // namespace internal
}  // namespace v8
