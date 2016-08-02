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

  static void DumpModule(WasmModule* module, ModuleResult result) {
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
      int string_leb_length = 0;
      uint32_t string_length =
          consume_u32v(&string_leb_length, "section name length");
      const byte* section_name_start = pc_;
      consume_bytes(string_length);
      if (failed()) {
        TRACE("Section name of length %u couldn't be read\n", string_length);
        break;
      }

      WasmSection::Code section =
          WasmSection::lookup(section_name_start, string_length);

      // Read and check the section size.
      int section_leb_length = 0;
      uint32_t section_length =
          consume_u32v(&section_leb_length, "section length");
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
          int length;
          module->min_mem_pages = consume_u32v(&length, "min memory");
          module->max_mem_pages = consume_u32v(&length, "max memory");
          module->mem_export = consume_u8("export memory") != 0;
          break;
        }
        case WasmSection::Code::Signatures: {
          int length;
          uint32_t signatures_count = consume_u32v(&length, "signatures count");
          module->signatures.reserve(SafeReserve(signatures_count));
          // Decode signatures.
          for (uint32_t i = 0; i < signatures_count; i++) {
            if (failed()) break;
            TRACE("DecodeSignature[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            FunctionSig* s = consume_sig();
            module->signatures.push_back(s);
          }
          break;
        }
        case WasmSection::Code::FunctionSignatures: {
          int length;
          uint32_t functions_count = consume_u32v(&length, "functions count");
          module->functions.reserve(SafeReserve(functions_count));
          for (uint32_t i = 0; i < functions_count; i++) {
            module->functions.push_back({nullptr,  // sig
                                         i,        // func_index
                                         0,        // sig_index
                                         0,        // name_offset
                                         0,        // name_length
                                         0,        // code_start_offset
                                         0,        // code_end_offset
                                         false});  // exported
            WasmFunction* function = &module->functions.back();
            function->sig_index = consume_sig_index(module, &function->sig);
          }
          break;
        }
        case WasmSection::Code::FunctionBodies: {
          int length;
          const byte* pos = pc_;
          uint32_t functions_count = consume_u32v(&length, "functions count");
          if (functions_count != module->functions.size()) {
            error(pos, pos, "function body count %u mismatch (%u expected)",
                  functions_count,
                  static_cast<uint32_t>(module->functions.size()));
            break;
          }
          for (uint32_t i = 0; i < functions_count; i++) {
            WasmFunction* function = &module->functions[i];
            int length;
            uint32_t size = consume_u32v(&length, "body size");
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
        case WasmSection::Code::OldFunctions: {
          int length;
          uint32_t functions_count = consume_u32v(&length, "functions count");
          module->functions.reserve(SafeReserve(functions_count));
          // Set up module environment for verification.
          ModuleEnv menv;
          menv.module = module;
          menv.instance = nullptr;
          menv.origin = origin_;
          // Decode functions.
          for (uint32_t i = 0; i < functions_count; i++) {
            if (failed()) break;
            TRACE("DecodeFunction[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));

            module->functions.push_back({nullptr,  // sig
                                         i,        // func_index
                                         0,        // sig_index
                                         0,        // name_offset
                                         0,        // name_length
                                         0,        // code_start_offset
                                         0,        // code_end_offset
                                         false});  // exported
            WasmFunction* function = &module->functions.back();
            DecodeFunctionInModule(module, function, false);
          }
          if (ok() && verify_functions) {
            for (uint32_t i = 0; i < functions_count; i++) {
              if (failed()) break;
              WasmFunction* function = &module->functions[i];
              VerifyFunctionBody(i, &menv, function);
              if (result_.failed()) {
                error(result_.error_pc, result_.error_msg.get());
              }
            }
          }
          break;
        }
        case WasmSection::Code::Names: {
          int length;
          const byte* pos = pc_;
          uint32_t functions_count = consume_u32v(&length, "functions count");
          if (functions_count != module->functions.size()) {
            error(pos, pos, "function name count %u mismatch (%u expected)",
                  functions_count,
                  static_cast<uint32_t>(module->functions.size()));
            break;
          }

          for (uint32_t i = 0; i < functions_count; i++) {
            WasmFunction* function = &module->functions[i];
            function->name_offset =
                consume_string(&function->name_length, false);

            uint32_t local_names_count =
                consume_u32v(&length, "local names count");
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
          int length;
          uint32_t globals_count = consume_u32v(&length, "globals count");
          module->globals.reserve(SafeReserve(globals_count));
          // Decode globals.
          for (uint32_t i = 0; i < globals_count; i++) {
            if (failed()) break;
            TRACE("DecodeGlobal[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            module->globals.push_back({0, 0, MachineType::Int32(), 0, false});
            WasmGlobal* global = &module->globals.back();
            DecodeGlobalInModule(global);
          }
          break;
        }
        case WasmSection::Code::DataSegments: {
          int length;
          uint32_t data_segments_count =
              consume_u32v(&length, "data segments count");
          module->data_segments.reserve(SafeReserve(data_segments_count));
          // Decode data segments.
          for (uint32_t i = 0; i < data_segments_count; i++) {
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
          int length;
          uint32_t function_table_count =
              consume_u32v(&length, "function table count");
          module->function_table.reserve(SafeReserve(function_table_count));
          // Decode function table.
          for (uint32_t i = 0; i < function_table_count; i++) {
            if (failed()) break;
            TRACE("DecodeFunctionTable[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            uint16_t index = consume_u32v(&length);
            if (index >= module->functions.size()) {
              error(pc_ - 2, "invalid function index");
              break;
            }
            module->function_table.push_back(index);
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
          int length;
          uint32_t import_table_count =
              consume_u32v(&length, "import table count");
          module->import_table.reserve(SafeReserve(import_table_count));
          // Decode import table.
          for (uint32_t i = 0; i < import_table_count; i++) {
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
          int length;
          uint32_t export_table_count =
              consume_u32v(&length, "export table count");
          module->export_table.reserve(SafeReserve(export_table_count));
          // Decode export table.
          for (uint32_t i = 0; i < export_table_count; i++) {
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
    ModuleResult result = toResult(module);
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
    function->exported = false;               // ---- exported

    if (ok()) VerifyFunctionBody(0, module_env, function);

    FunctionResult result;
    result.CopyFrom(result_);  // Copy error code and location.
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
    DCHECK(unibrow::Utf8::Validate(start_ + global->name_offset,
                                   global->name_length));
    global->type = mem_type();
    global->offset = 0;
    global->exported = consume_u8("exported") != 0;
  }

  // Decodes a single function entry inside a module starting at {pc_}.
  // TODO(titzer): legacy function body; remove
  void DecodeFunctionInModule(WasmModule* module, WasmFunction* function,
                              bool verify_body = true) {
    byte decl_bits = consume_u8("function decl");

    const byte* sigpos = pc_;
    function->sig_index = consume_u16("signature index");

    if (function->sig_index >= module->signatures.size()) {
      return error(sigpos, "invalid signature index");
    } else {
      function->sig = module->signatures[function->sig_index];
    }

    TRACE("  +%d  <function attributes:%s%s>\n", static_cast<int>(pc_ - start_),
          decl_bits & kDeclFunctionName ? " name" : "",
          decl_bits & kDeclFunctionExport ? " exported" : "");

    function->exported = decl_bits & kDeclFunctionExport;

    if (decl_bits & kDeclFunctionName) {
      function->name_offset =
          consume_string(&function->name_length, function->exported);
    }

    uint16_t size = consume_u16("body size");
    if (ok()) {
      if ((pc_ + size) > limit_) {
        return error(pc_, limit_,
                     "expected %d bytes for function body, fell off end", size);
      }
      function->code_start_offset = static_cast<uint32_t>(pc_ - start_);
      function->code_end_offset = function->code_start_offset + size;
      TRACE("  +%d  %-20s: (%d bytes)\n", static_cast<int>(pc_ - start_),
            "function body", size);
      pc_ += size;
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
    int length;
    segment->dest_addr = consume_u32v(&length, "destination");
    segment->source_size = consume_u32v(&length, "source size");
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
    TreeResult result = VerifyWasmCode(module_zone->allocator(), body);
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
      result_.CopyFrom(result);
      result_.error_msg.Reset(buffer);
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
    int varint_length;
    *length = consume_u32v(&varint_length, "string length");
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
    int length;
    uint32_t sig_index = consume_u32v(&length, "signature index");
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
    int length;
    uint32_t func_index = consume_u32v(&length, "function index");
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

  // Reads a single 8-bit integer, interpreting it as a memory type.
  MachineType mem_type() {
    byte val = consume_u8("memory type");
    MemTypeCode t = static_cast<MemTypeCode>(val);
    switch (t) {
      case kMemI8:
        return MachineType::Int8();
      case kMemU8:
        return MachineType::Uint8();
      case kMemI16:
        return MachineType::Int16();
      case kMemU16:
        return MachineType::Uint16();
      case kMemI32:
        return MachineType::Int32();
      case kMemU32:
        return MachineType::Uint32();
      case kMemI64:
        return MachineType::Int64();
      case kMemU64:
        return MachineType::Uint64();
      case kMemF32:
        return MachineType::Float32();
      case kMemF64:
        return MachineType::Float64();
      default:
        error(pc_ - 1, "invalid memory type");
        return MachineType::None();
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
    int length;
    // parse parameter types
    uint32_t param_count = consume_u32v(&length, "param count");
    std::vector<LocalType> params;
    for (uint32_t i = 0; i < param_count; i++) {
      LocalType param = consume_local_type();
      if (param == kAstStmt) error(pc_ - 1, "invalid void parameter type");
      params.push_back(param);
    }

    // parse return types
    const byte* pt = pc_;
    uint32_t return_count = consume_u32v(&length, "return count");
    if (return_count > kMaxReturnCount) {
      error(pt, pt, "return count of %u exceeds maximum of %u", return_count,
            kMaxReturnCount);
      return nullptr;
    }
    std::vector<LocalType> returns;
    for (uint32_t i = 0; i < return_count; i++) {
      LocalType ret = consume_local_type();
      if (ret == kAstStmt) error(pc_ - 1, "invalid void return type");
      returns.push_back(ret);
    }

    // FunctionSig stores the return types first.
    LocalType* buffer =
        module_zone->NewArray<LocalType>(param_count + return_count);
    uint32_t b = 0;
    for (uint32_t i = 0; i < return_count; i++) buffer[b++] = returns[i];
    for (uint32_t i = 0; i < param_count; i++) buffer[b++] = params[i];

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
    error_msg.Reset(result);
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
    error_msg.Reset(result);
  }
};

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
}  // namespace wasm
}  // namespace internal
}  // namespace v8
