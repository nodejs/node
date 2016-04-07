// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/macro-assembler.h"
#include "src/objects.h"
#include "src/v8.h"

#include "src/wasm/decoder.h"
#include "src/wasm/module-decoder.h"

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
                bool asm_js)
      : Decoder(module_start, module_end), module_zone(zone), asm_js_(asm_js) {
    result_.start = start_;
    if (limit_ < start_) {
      error(start_, "end is less than start");
      limit_ = start_;
    }
  }

  virtual void onFirstError() {
    pc_ = limit_;  // On error, terminate section decoding loop.
  }

  // Decodes an entire module.
  ModuleResult DecodeModule(WasmModule* module, bool verify_functions = true) {
    pc_ = start_;
    module->module_start = start_;
    module->module_end = limit_;
    module->min_mem_size_log2 = 0;
    module->max_mem_size_log2 = 0;
    module->mem_export = false;
    module->mem_external = false;
    module->globals = new std::vector<WasmGlobal>();
    module->signatures = new std::vector<FunctionSig*>();
    module->functions = new std::vector<WasmFunction>();
    module->data_segments = new std::vector<WasmDataSegment>();
    module->function_table = new std::vector<uint16_t>();
    module->import_table = new std::vector<WasmImport>();

    bool sections[kMaxModuleSectionCode];
    memset(sections, 0, sizeof(sections));

    // Decode the module sections.
    while (pc_ < limit_) {
      TRACE("DecodeSection\n");
      WasmSectionDeclCode section =
          static_cast<WasmSectionDeclCode>(consume_u8("section"));
      // Each section should appear at most once.
      if (section < kMaxModuleSectionCode) {
        CheckForPreviousSection(sections, section, false);
        sections[section] = true;
      }

      switch (section) {
        case kDeclEnd:
          // Terminate section decoding.
          limit_ = pc_;
          break;
        case kDeclMemory:
          module->min_mem_size_log2 = consume_u8("min memory");
          module->max_mem_size_log2 = consume_u8("max memory");
          module->mem_export = consume_u8("export memory") != 0;
          break;
        case kDeclSignatures: {
          int length;
          uint32_t signatures_count = consume_u32v(&length, "signatures count");
          module->signatures->reserve(SafeReserve(signatures_count));
          // Decode signatures.
          for (uint32_t i = 0; i < signatures_count; i++) {
            if (failed()) break;
            TRACE("DecodeSignature[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            FunctionSig* s = consume_sig();  // read function sig.
            module->signatures->push_back(s);
          }
          break;
        }
        case kDeclFunctions: {
          // Functions require a signature table first.
          CheckForPreviousSection(sections, kDeclSignatures, true);
          int length;
          uint32_t functions_count = consume_u32v(&length, "functions count");
          module->functions->reserve(SafeReserve(functions_count));
          // Set up module environment for verification.
          ModuleEnv menv;
          menv.module = module;
          menv.instance = nullptr;
          menv.asm_js = asm_js_;
          // Decode functions.
          for (uint32_t i = 0; i < functions_count; i++) {
            if (failed()) break;
            TRACE("DecodeFunction[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));

            module->functions->push_back(
                {nullptr, i, 0, 0, 0, 0, 0, 0, false, false});
            WasmFunction* function = &module->functions->back();
            DecodeFunctionInModule(module, function, false);
          }
          if (ok() && verify_functions) {
            for (uint32_t i = 0; i < functions_count; i++) {
              if (failed()) break;
              WasmFunction* function = &module->functions->at(i);
              if (!function->external) {
                VerifyFunctionBody(i, &menv, function);
                if (result_.failed())
                  error(result_.error_pc, result_.error_msg.get());
              }
            }
          }
          break;
        }
        case kDeclGlobals: {
          int length;
          uint32_t globals_count = consume_u32v(&length, "globals count");
          module->globals->reserve(SafeReserve(globals_count));
          // Decode globals.
          for (uint32_t i = 0; i < globals_count; i++) {
            if (failed()) break;
            TRACE("DecodeGlobal[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            module->globals->push_back({0, MachineType::Int32(), 0, false});
            WasmGlobal* global = &module->globals->back();
            DecodeGlobalInModule(global);
          }
          break;
        }
        case kDeclDataSegments: {
          int length;
          uint32_t data_segments_count =
              consume_u32v(&length, "data segments count");
          module->data_segments->reserve(SafeReserve(data_segments_count));
          // Decode data segments.
          for (uint32_t i = 0; i < data_segments_count; i++) {
            if (failed()) break;
            TRACE("DecodeDataSegment[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            module->data_segments->push_back({0, 0, 0});
            WasmDataSegment* segment = &module->data_segments->back();
            DecodeDataSegmentInModule(module, segment);
          }
          break;
        }
        case kDeclFunctionTable: {
          // An indirect function table requires functions first.
          CheckForPreviousSection(sections, kDeclFunctions, true);
          int length;
          uint32_t function_table_count =
              consume_u32v(&length, "function table count");
          module->function_table->reserve(SafeReserve(function_table_count));
          // Decode function table.
          for (uint32_t i = 0; i < function_table_count; i++) {
            if (failed()) break;
            TRACE("DecodeFunctionTable[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));
            uint16_t index = consume_u16();
            if (index >= module->functions->size()) {
              error(pc_ - 2, "invalid function index");
              break;
            }
            module->function_table->push_back(index);
          }
          break;
        }
        case kDeclStartFunction: {
          // Declares a start function for a module.
          CheckForPreviousSection(sections, kDeclFunctions, true);
          if (module->start_function_index >= 0) {
            error("start function already declared");
            break;
          }
          int length;
          const byte* before = pc_;
          uint32_t index = consume_u32v(&length, "start function index");
          if (index >= module->functions->size()) {
            error(before, "invalid start function index");
            break;
          }
          module->start_function_index = static_cast<int>(index);
          FunctionSig* sig =
              module->signatures->at(module->functions->at(index).sig_index);
          if (sig->parameter_count() > 0) {
            error(before, "invalid start function: non-zero parameter count");
            break;
          }
          break;
        }
        case kDeclImportTable: {
          // Declares an import table.
          CheckForPreviousSection(sections, kDeclSignatures, true);
          int length;
          uint32_t import_table_count =
              consume_u32v(&length, "import table count");
          module->import_table->reserve(SafeReserve(import_table_count));
          // Decode import table.
          for (uint32_t i = 0; i < import_table_count; i++) {
            if (failed()) break;
            TRACE("DecodeImportTable[%d] module+%d\n", i,
                  static_cast<int>(pc_ - start_));

            module->import_table->push_back({nullptr, 0, 0});
            WasmImport* import = &module->import_table->back();

            const byte* sigpos = pc_;
            import->sig_index = consume_u16("signature index");

            if (import->sig_index >= module->signatures->size()) {
              error(sigpos, "invalid signature index");
            } else {
              import->sig = module->signatures->at(import->sig_index);
            }
            import->module_name_offset = consume_string("import module name");
            import->function_name_offset =
                consume_string("import function name");
          }
          break;
        }
        case kDeclWLL: {
          // Reserved for experimentation by the Web Low-level Language project
          // which is augmenting the binary encoding with source code meta
          // information. This section does not affect the semantics of the code
          // and can be ignored by the runtime. https://github.com/JSStats/wll
          int length = 0;
          uint32_t section_size = consume_u32v(&length, "section size");
          if (pc_ + section_size > limit_ || pc_ + section_size < pc_) {
            error(pc_ - length, "invalid section size");
            break;
          }
          pc_ += section_size;
          break;
        }
        default:
          error(pc_ - 1, nullptr, "unrecognized section 0x%02x", section);
          break;
      }
    }

    return toResult(module);
  }

  uint32_t SafeReserve(uint32_t count) {
    // Avoid OOM by only reserving up to a certain size.
    const uint32_t kMaxReserve = 20000;
    return count < kMaxReserve ? count : kMaxReserve;
  }

  void CheckForPreviousSection(bool* sections, WasmSectionDeclCode section,
                               bool present) {
    if (section >= kMaxModuleSectionCode) return;
    if (sections[section] == present) return;
    const char* name = "";
    switch (section) {
      case kDeclMemory:
        name = "memory";
        break;
      case kDeclSignatures:
        name = "signatures";
        break;
      case kDeclFunctions:
        name = "function declaration";
        break;
      case kDeclGlobals:
        name = "global variable";
        break;
      case kDeclDataSegments:
        name = "data segment";
        break;
      case kDeclFunctionTable:
        name = "function table";
        break;
      default:
        name = "";
        break;
    }
    if (present) {
      error(pc_ - 1, nullptr, "required %s section missing", name);
    } else {
      error(pc_ - 1, nullptr, "%s section already present", name);
    }
  }

  // Decodes a single anonymous function starting at {start_}.
  FunctionResult DecodeSingleFunction(ModuleEnv* module_env,
                                      WasmFunction* function) {
    pc_ = start_;
    function->sig = consume_sig();                  // read signature
    function->name_offset = 0;                   // ---- name
    function->code_start_offset = off(pc_ + 8);  // ---- code start
    function->code_end_offset = off(limit_);     // ---- code end
    function->local_i32_count = consume_u16();      // read u16
    function->local_i64_count = consume_u16();      // read u16
    function->local_f32_count = consume_u16();      // read u16
    function->local_f64_count = consume_u16();      // read u16
    function->exported = false;                  // ---- exported
    function->external = false;                  // ---- external

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
  bool asm_js_;

  uint32_t off(const byte* ptr) { return static_cast<uint32_t>(ptr - start_); }

  // Decodes a single global entry inside a module starting at {pc_}.
  void DecodeGlobalInModule(WasmGlobal* global) {
    global->name_offset = consume_string("global name");
    global->type = mem_type();
    global->offset = 0;
    global->exported = consume_u8("exported") != 0;
  }

  // Decodes a single function entry inside a module starting at {pc_}.
  void DecodeFunctionInModule(WasmModule* module, WasmFunction* function,
                              bool verify_body = true) {
    byte decl_bits = consume_u8("function decl");

    const byte* sigpos = pc_;
    function->sig_index = consume_u16("signature index");

    if (function->sig_index >= module->signatures->size()) {
      return error(sigpos, "invalid signature index");
    } else {
      function->sig = module->signatures->at(function->sig_index);
    }

    TRACE("  +%d  <function attributes:%s%s%s%s%s>\n",
          static_cast<int>(pc_ - start_),
          decl_bits & kDeclFunctionName ? " name" : "",
          decl_bits & kDeclFunctionImport ? " imported" : "",
          decl_bits & kDeclFunctionLocals ? " locals" : "",
          decl_bits & kDeclFunctionExport ? " exported" : "",
          (decl_bits & kDeclFunctionImport) == 0 ? " body" : "");

    if (decl_bits & kDeclFunctionName) {
      function->name_offset = consume_string("function name");
    }

    function->exported = decl_bits & kDeclFunctionExport;

    // Imported functions have no locals or body.
    if (decl_bits & kDeclFunctionImport) {
      function->external = true;
      return;
    }

    if (decl_bits & kDeclFunctionLocals) {
      function->local_i32_count = consume_u16("i32 count");
      function->local_i64_count = consume_u16("i64 count");
      function->local_f32_count = consume_u16("f32 count");
      function->local_f64_count = consume_u16("f64 count");
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
    segment->dest_addr = consume_u32("destination");
    segment->source_offset = consume_offset("source offset");
    segment->source_size = consume_u32("source size");
    segment->init = consume_u8("init");

    // Validate the data is in the module.
    uint32_t module_limit = static_cast<uint32_t>(limit_ - start_);
    if (!IsWithinLimit(module_limit, segment->source_offset,
                       segment->source_size)) {
      error(pc_ - sizeof(uint32_t), "segment out of bounds of module");
    }

    // Validate that the segment will fit into the (minimum) memory.
    uint32_t memory_limit =
        1 << (module ? module->min_mem_size_log2 : WasmModule::kMaxMemSize);
    if (!IsWithinLimit(memory_limit, segment->dest_addr,
                       segment->source_size)) {
      error(pc_ - sizeof(uint32_t), "segment out of bounds of memory");
    }
  }

  // Verifies the body (code) of a given function.
  void VerifyFunctionBody(uint32_t func_num, ModuleEnv* menv,
                          WasmFunction* function) {
    if (FLAG_trace_wasm_decode_time) {
      OFStream os(stdout);
      os << "Verifying WASM function " << WasmFunctionName(function, menv)
         << std::endl;
      os << std::endl;
    }
    FunctionEnv fenv;
    fenv.module = menv;
    fenv.sig = function->sig;
    fenv.local_i32_count = function->local_i32_count;
    fenv.local_i64_count = function->local_i64_count;
    fenv.local_f32_count = function->local_f32_count;
    fenv.local_f64_count = function->local_f64_count;
    fenv.SumLocals();

    TreeResult result =
        VerifyWasmCode(&fenv, start_, start_ + function->code_start_offset,
                       start_ + function->code_end_offset);
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

  // Reads a single 32-bit unsigned integer interpreted as an offset into the
  // data and validating the string there and advances.
  uint32_t consume_string(const char* name = nullptr) {
    // TODO(titzer): validate string
    return consume_offset(name ? name : "string");
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

  // Parses an inline function signature.
  FunctionSig* consume_sig() {
    byte count = consume_u8("param count");
    LocalType ret = consume_local_type();
    FunctionSig::Builder builder(module_zone, ret == kAstStmt ? 0 : 1, count);
    if (ret != kAstStmt) builder.AddReturn(ret);

    for (int i = 0; i < count; i++) {
      LocalType param = consume_local_type();
      if (param == kAstStmt) error(pc_ - 1, "invalid void parameter type");
      builder.AddParam(param);
    }
    return builder.Build();
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
                              bool verify_functions, bool asm_js) {
  size_t size = module_end - module_start;
  if (module_start > module_end) return ModuleError("start > end");
  if (size >= kMaxModuleSize) return ModuleError("size > maximum module size");
  WasmModule* module = new WasmModule();
  ModuleDecoder decoder(zone, module_start, module_end, asm_js);
  return decoder.DecodeModule(module, verify_functions);
}


FunctionSig* DecodeWasmSignatureForTesting(Zone* zone, const byte* start,
                                           const byte* end) {
  ModuleDecoder decoder(zone, start, end, false);
  return decoder.DecodeFunctionSignature(start);
}


FunctionResult DecodeWasmFunction(Isolate* isolate, Zone* zone,
                                  ModuleEnv* module_env,
                                  const byte* function_start,
                                  const byte* function_end) {
  size_t size = function_end - function_start;
  if (function_start > function_end) return FunctionError("start > end");
  if (size > kMaxFunctionSize)
    return FunctionError("size > maximum function size");
  WasmFunction* function = new WasmFunction();
  ModuleDecoder decoder(zone, function_start, function_end, false);
  return decoder.DecodeSingleFunction(module_env, function);
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
