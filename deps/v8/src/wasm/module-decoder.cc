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
    module->mem_external = false;
    module->origin = origin_;

    const byte* pos = pc_;
    int current_order = 0;
    uint32_t magic_word = consume_u32("wasm magic");
#define BYTES(x) (x & 0xff), (x >> 8) & 0xff, (x >> 16) & 0xff, (x >> 24) & 0xff
    if (magic_word != kWasmMagic) {
      error(pos, pos,
            "expected magic word %02x %02x %02x %02x, "
            "found %02x %02x %02x %02x",
            BYTES(kWasmMagic), BYTES(magic_word));
      goto done;
    }

    pos = pc_;
    {
      uint32_t magic_version = consume_u32("wasm version");
      if (magic_version != kWasmVersion) {
        error(pos, pos,
              "expected version %02x %02x %02x %02x, "
              "found %02x %02x %02x %02x",
              BYTES(kWasmVersion), BYTES(magic_version));
        goto done;
      }
    }

    // Decode the module sections.
    while (pc_ < limit_) {
      TRACE("DecodeSection\n");
      pos = pc_;

      // Read the section name.
      uint32_t string_length = consume_u32v("section name length");
      const byte* section_name_start = pc_;
      consume_bytes(string_length);
      if (failed()) {
        TRACE("Section name of length %u couldn't be read\n", string_length);
        break;
      }

      TRACE("  +%d  section name        : \"%.*s\"\n",
            static_cast<int>(section_name_start - start_),
            string_length < 20 ? string_length : 20, section_name_start);

      WasmSection::Code section =
          WasmSection::lookup(section_name_start, string_length);

      // Read and check the section size.
      uint32_t section_length = consume_u32v("section length");
      if (!checkAvailable(section_length)) {
        // The section would extend beyond the end of the module.
        break;
      }
      const byte* section_start = pc_;
      const byte* expected_section_end = pc_ + section_length;

      current_order = CheckSectionOrder(current_order, section);

      switch (section) {
        case WasmSection::Code::End:
          // Terminate section decoding.
          limit_ = pc_;
          break;
        case WasmSection::Code::Memory: {
          module->min_mem_pages = consume_u32v("min memory");
          module->max_mem_pages = consume_u32v("max memory");
          module->mem_export = consume_u8("export memory") != 0;
          break;
        }
        case WasmSection::Code::Signatures: {
          uint32_t signatures_count = consume_u32v("signatures count");
          module->signatures.reserve(SafeReserve(signatures_count));
          // Decode signatures.
          for (uint32_t i = 0; i < signatures_count; ++i) {
            if (failed()) break;
            TRACE("DecodeSignature[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            FunctionSig* s = consume_sig();
            module->signatures.push_back(s);
          }
          break;
        }
        case WasmSection::Code::FunctionSignatures: {
          uint32_t functions_count = consume_u32v("functions count");
          module->functions.reserve(SafeReserve(functions_count));
          for (uint32_t i = 0; i < functions_count; ++i) {
            module->functions.push_back({nullptr,  // sig
                                         i,        // func_index
                                         0,        // sig_index
                                         0,        // name_offset
                                         0,        // name_length
                                         0,        // code_start_offset
                                         0});      // code_end_offset
            WasmFunction* function = &module->functions.back();
            function->sig_index = consume_sig_index(module, &function->sig);
          }
          break;
        }
        case WasmSection::Code::FunctionBodies: {
          const byte* pos = pc_;
          uint32_t functions_count = consume_u32v("functions count");
          if (functions_count != module->functions.size()) {
            error(pos, pos, "function body count %u mismatch (%u expected)",
                  functions_count,
                  static_cast<uint32_t>(module->functions.size()));
            break;
          }
          for (uint32_t i = 0; i < functions_count; ++i) {
            WasmFunction* function = &module->functions[i];
            uint32_t size = consume_u32v("body size");
            function->code_start_offset = pc_offset();
            function->code_end_offset = pc_offset() + size;

            TRACE("  +%d  %-20s: (%d bytes)\n", pc_offset(), "function body",
                  size);
            pc_ += size;
            if (pc_ > limit_) {
              error(pc_, "function body extends beyond end of file");
            }
          }
          break;
        }
        case WasmSection::Code::Names: {
          const byte* pos = pc_;
          uint32_t functions_count = consume_u32v("functions count");
          if (functions_count != module->functions.size()) {
            error(pos, pos, "function name count %u mismatch (%u expected)",
                  functions_count,
                  static_cast<uint32_t>(module->functions.size()));
            break;
          }

          for (uint32_t i = 0; i < functions_count; ++i) {
            WasmFunction* function = &module->functions[i];
            function->name_offset =
                consume_string(&function->name_length, false);

            uint32_t local_names_count = consume_u32v("local names count");
            for (uint32_t j = 0; j < local_names_count; j++) {
              uint32_t unused = 0;
              uint32_t offset = consume_string(&unused, false);
              USE(unused);
              USE(offset);
            }
          }
          break;
        }
        case WasmSection::Code::Globals: {
          uint32_t globals_count = consume_u32v("globals count");
          module->globals.reserve(SafeReserve(globals_count));
          // Decode globals.
          for (uint32_t i = 0; i < globals_count; ++i) {
            if (failed()) break;
            TRACE("DecodeGlobal[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            // Add an uninitialized global and pass a pointer to it.
            module->globals.push_back({0, 0, kAstStmt, 0, false});
            WasmGlobal* global = &module->globals.back();
            DecodeGlobalInModule(global);
          }
          break;
        }
        case WasmSection::Code::DataSegments: {
          uint32_t data_segments_count = consume_u32v("data segments count");
          module->data_segments.reserve(SafeReserve(data_segments_count));
          // Decode data segments.
          for (uint32_t i = 0; i < data_segments_count; ++i) {
            if (failed()) break;
            TRACE("DecodeDataSegment[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            module->data_segments.push_back({0,        // dest_addr
                                             0,        // source_offset
                                             0,        // source_size
                                             false});  // init
            WasmDataSegment* segment = &module->data_segments.back();
            DecodeDataSegmentInModule(module, segment);
          }
          break;
        }
        case WasmSection::Code::FunctionTable: {
          // An indirect function table requires functions first.
          CheckForFunctions(module, section);
          // Assume only one table for now.
          static const uint32_t kSupportedTableCount = 1;
          module->function_tables.reserve(SafeReserve(kSupportedTableCount));
          // Decode function table.
          for (uint32_t i = 0; i < kSupportedTableCount; ++i) {
            if (failed()) break;
            TRACE("DecodeFunctionTable[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            module->function_tables.push_back({0, 0, std::vector<uint16_t>()});
            DecodeFunctionTableInModule(module, &module->function_tables[i]);
          }
          break;
        }
        case WasmSection::Code::StartFunction: {
          // Declares a start function for a module.
          CheckForFunctions(module, section);
          if (module->start_function_index >= 0) {
            error("start function already declared");
            break;
          }
          WasmFunction* func;
          const byte* pos = pc_;
          module->start_function_index = consume_func_index(module, &func);
          if (func && func->sig->parameter_count() > 0) {
            error(pos, "invalid start function: non-zero parameter count");
            break;
          }
          break;
        }
        case WasmSection::Code::ImportTable: {
          uint32_t import_table_count = consume_u32v("import table count");
          module->import_table.reserve(SafeReserve(import_table_count));
          // Decode import table.
          for (uint32_t i = 0; i < import_table_count; ++i) {
            if (failed()) break;
            TRACE("DecodeImportTable[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));

            module->import_table.push_back({nullptr,  // sig
                                            0,        // sig_index
                                            0,        // module_name_offset
                                            0,        // module_name_length
                                            0,        // function_name_offset
                                            0});      // function_name_length
            WasmImport* import = &module->import_table.back();

            import->sig_index = consume_sig_index(module, &import->sig);
            const byte* pos = pc_;
            import->module_name_offset =
                consume_string(&import->module_name_length, true);
            if (import->module_name_length == 0) {
              error(pos, "import module name cannot be NULL");
            }
            import->function_name_offset =
                consume_string(&import->function_name_length, true);
          }
          break;
        }
        case WasmSection::Code::ExportTable: {
          // Declares an export table.
          CheckForFunctions(module, section);
          uint32_t export_table_count = consume_u32v("export table count");
          module->export_table.reserve(SafeReserve(export_table_count));
          // Decode export table.
          for (uint32_t i = 0; i < export_table_count; ++i) {
            if (failed()) break;
            TRACE("DecodeExportTable[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));

            module->export_table.push_back({0,    // func_index
                                            0,    // name_offset
                                            0});  // name_length
            WasmExport* exp = &module->export_table.back();

            WasmFunction* func;
            exp->func_index = consume_func_index(module, &func);
            exp->name_offset = consume_string(&exp->name_length, true);
          }
          // Check for duplicate exports.
          if (ok() && module->export_table.size() > 1) {
            std::vector<WasmExport> sorted_exports(module->export_table);
            const byte* base = start_;
            auto cmp_less = [base](const WasmExport& a, const WasmExport& b) {
              // Return true if a < b.
              uint32_t len = a.name_length;
              if (len != b.name_length) return len < b.name_length;
              return memcmp(base + a.name_offset, base + b.name_offset, len) <
                     0;
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
                      it->name_length, pc, last->func_index, it->func_index);
                break;
              }
            }
          }
          break;
        }
        case WasmSection::Code::Max:
          // Skip unknown sections.
          TRACE("Unknown section: '");
          for (uint32_t i = 0; i != string_length; ++i) {
            TRACE("%c", *(section_name_start + i));
          }
          TRACE("'\n");
          consume_bytes(section_length);
          break;
      }

      if (pc_ != expected_section_end) {
        const char* diff = pc_ < expected_section_end ? "shorter" : "longer";
        size_t expected_length = static_cast<size_t>(section_length);
        size_t actual_length = static_cast<size_t>(pc_ - section_start);
        error(pc_, pc_,
              "section \"%s\" %s (%zu bytes) than specified (%zu bytes)",
              WasmSection::getName(section), diff, actual_length,
              expected_length);
        break;
      }
    }

  done:
    if (ok()) CalculateGlobalsOffsets(module);
    const WasmModule* finished_module = module;
    ModuleResult result = toResult(finished_module);
    if (FLAG_dump_wasm_module) {
      DumpModule(module, result);
    }
    return result;
  }

  uint32_t SafeReserve(uint32_t count) {
    // Avoid OOM by only reserving up to a certain size.
    const uint32_t kMaxReserve = 20000;
    return count < kMaxReserve ? count : kMaxReserve;
  }

  void CheckForFunctions(WasmModule* module, WasmSection::Code section) {
    if (module->functions.size() == 0) {
      error(pc_ - 1, nullptr, "functions must appear before section %s",
            WasmSection::getName(section));
    }
  }

  int CheckSectionOrder(int current_order, WasmSection::Code section) {
    int next_order = WasmSection::getOrder(section);
    if (next_order == 0) return current_order;
    if (next_order == current_order) {
      error(pc_, pc_, "section \"%s\" already defined",
            WasmSection::getName(section));
    }
    if (next_order < current_order) {
      error(pc_, pc_, "section \"%s\" out of order",
            WasmSection::getName(section));
    }
    return next_order;
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

 private:
  Zone* module_zone;
  ModuleResult result_;
  ModuleOrigin origin_;

  uint32_t off(const byte* ptr) { return static_cast<uint32_t>(ptr - start_); }

  // Decodes a single global entry inside a module starting at {pc_}.
  void DecodeGlobalInModule(WasmGlobal* global) {
    global->name_offset = consume_string(&global->name_length, false);
    if (!unibrow::Utf8::Validate(start_ + global->name_offset,
                                 global->name_length)) {
      error("global name is not valid utf8");
    }
    global->type = consume_local_type();
    global->offset = 0;
    global->exported = consume_u8("exported") != 0;
  }

  bool IsWithinLimit(uint32_t limit, uint32_t offset, uint32_t size) {
    if (offset > limit) return false;
    if ((offset + size) < offset) return false;  // overflow
    return (offset + size) <= limit;
  }

  // Decodes a single data segment entry inside a module starting at {pc_}.
  void DecodeDataSegmentInModule(WasmModule* module, WasmDataSegment* segment) {
    const byte* start = pc_;
    segment->dest_addr = consume_u32v("destination");
    segment->source_size = consume_u32v("source size");
    segment->source_offset = static_cast<uint32_t>(pc_ - start_);
    segment->init = true;

    // Validate the data is in the module.
    uint32_t module_limit = static_cast<uint32_t>(limit_ - start_);
    if (!IsWithinLimit(module_limit, segment->source_offset,
                       segment->source_size)) {
      error(start, "segment out of bounds of module");
    }

    // Validate that the segment will fit into the (minimum) memory.
    uint32_t memory_limit =
        WasmModule::kPageSize * (module ? module->min_mem_pages
                                        : WasmModule::kMaxMemPages);
    if (!IsWithinLimit(memory_limit, segment->dest_addr,
                       segment->source_size)) {
      error(start, "segment out of bounds of memory");
    }

    consume_bytes(segment->source_size);
  }

  // Decodes a single function table inside a module starting at {pc_}.
  void DecodeFunctionTableInModule(WasmModule* module,
                                   WasmIndirectFunctionTable* table) {
    table->size = consume_u32v("function table entry count");
    table->max_size = table->size;

    if (table->max_size != table->size) {
      error("invalid table maximum size");
    }

    for (uint32_t i = 0; i < table->size; ++i) {
      uint16_t index = consume_u32v();
      if (index >= module->functions.size()) {
        error(pc_ - sizeof(index), "invalid function index");
        break;
      }
      table->values.push_back(index);
    }
  }

  // Calculate individual global offsets and total size of globals table.
  void CalculateGlobalsOffsets(WasmModule* module) {
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

  // Reads a single 32-bit unsigned integer interpreted as an offset, checking
  // the offset is within bounds and advances.
  uint32_t consume_offset(const char* name = nullptr) {
    uint32_t offset = consume_u32(name ? name : "offset");
    if (offset > static_cast<uint32_t>(limit_ - start_)) {
      error(pc_ - sizeof(uint32_t), "offset out of bounds of module");
    }
    return offset;
  }

  // Reads a length-prefixed string, checking that it is within bounds. Returns
  // the offset of the string, and the length as an out parameter.
  uint32_t consume_string(uint32_t* length, bool validate_utf8) {
    *length = consume_u32v("string length");
    uint32_t offset = pc_offset();
    TRACE("  +%u  %-20s: (%u bytes)\n", offset, "string", *length);
    if (validate_utf8 && !unibrow::Utf8::Validate(pc_, *length)) {
      error(pc_, "no valid UTF-8 string");
    }
    consume_bytes(*length);
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
    const byte* pos = pc_;
    uint32_t func_index = consume_u32v("function index");
    if (func_index >= module->functions.size()) {
      error(pos, pos, "function index %u out of bounds (%d functions)",
            func_index, static_cast<int>(module->functions.size()));
      *func = nullptr;
      return 0;
    }
    *func = &module->functions[func_index];
    return func_index;
  }

  // Reads a single 8-bit integer, interpreting it as a local type.
  LocalType consume_local_type() {
    byte val = consume_u8("local type");
    LocalTypeCode t = static_cast<LocalTypeCode>(val);
    switch (t) {
      case kLocalVoid:
        return kAstStmt;
      case kLocalI32:
        return kAstI32;
      case kLocalI64:
        return kAstI64;
      case kLocalF32:
        return kAstF32;
      case kLocalF64:
        return kAstF64;
      default:
        error(pc_ - 1, "invalid local type");
        return kAstStmt;
    }
  }

  // Parses a type entry, which is currently limited to functions only.
  FunctionSig* consume_sig() {
    const byte* pos = pc_;
    byte form = consume_u8("type form");
    if (form != kWasmFunctionTypeForm) {
      error(pos, pos, "expected function type form (0x%02x), got: 0x%02x",
            kWasmFunctionTypeForm, form);
      return nullptr;
    }
    // parse parameter types
    uint32_t param_count = consume_u32v("param count");
    std::vector<LocalType> params;
    for (uint32_t i = 0; i < param_count; ++i) {
      LocalType param = consume_local_type();
      if (param == kAstStmt) error(pc_ - 1, "invalid void parameter type");
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
    for (uint32_t i = 0; i < return_count; ++i) {
      LocalType ret = consume_local_type();
      if (ret == kAstStmt) error(pc_ - 1, "invalid void return type");
      returns.push_back(ret);
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
                               WasmSection::Code code) {
  Decoder decoder(module_start, module_end);

  uint32_t magic_word = decoder.consume_u32("wasm magic");
  if (magic_word != kWasmMagic) decoder.error("wrong magic word");

  uint32_t magic_version = decoder.consume_u32("wasm version");
  if (magic_version != kWasmVersion) decoder.error("wrong wasm version");

  while (decoder.more() && decoder.ok()) {
    // Read the section name.
    uint32_t string_length = decoder.consume_u32v("section name length");
    const byte* section_name_start = decoder.pc();
    decoder.consume_bytes(string_length);
    if (decoder.failed()) break;

    WasmSection::Code section =
        WasmSection::lookup(section_name_start, string_length);

    // Read and check the section size.
    uint32_t section_length = decoder.consume_u32v("section length");

    const byte* section_start = decoder.pc();
    decoder.consume_bytes(section_length);
    if (section == code && decoder.ok()) {
      return Vector<const uint8_t>(section_start, section_length);
    }
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

FunctionOffsetsResult DecodeWasmFunctionOffsets(const byte* module_start,
                                                const byte* module_end) {
  Vector<const byte> code_section =
      FindSection(module_start, module_end, WasmSection::Code::FunctionBodies);
  Decoder decoder(code_section.start(), code_section.end());
  if (!code_section.start()) decoder.error("no code section");

  uint32_t functions_count = decoder.consume_u32v("functions count");
  FunctionOffsets table;
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
