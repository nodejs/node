// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/wasm_module.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "experimental/redline/core/random.h"
#include "experimental/redline/core/tunable.h"

namespace redline {

REDLINE_TUNABLE(kWasmMaxImports, "generators.WasmModule.max_imports", 5, 0, 20,
                "max imported functions");
REDLINE_TUNABLE(kWasmMaxFunctions, "generators.WasmModule.max_functions", 5, 1,
                50, "max defined functions");
REDLINE_TUNABLE(kWasmMaxGlobals, "generators.WasmModule.max_globals", 10, 0, 50,
                "max globals");
REDLINE_TUNABLE(kWasmMaxMemoryPages, "generators.WasmModule.max_memory_pages",
                2, 1, 10, "max memory pages");
REDLINE_TUNABLE(kWasmMemoryProb, "generators.WasmModule.memory_probability",
                0.5, 0, 1, "probability of memory");
REDLINE_TUNABLE(kWasmTableProb, "generators.WasmModule.table_probability", 0.5,
                0, 1, "probability of table");
REDLINE_TUNABLE(kWasmMaxParams, "generators.WasmModule.max_params", 3, 0, 10,
                "max function parameters");
REDLINE_TUNABLE(kWasmMaxLocals, "generators.WasmModule.max_locals", 5, 0, 20,
                "max function locals");
REDLINE_TUNABLE(kWasmMaxExprDepth, "generators.WasmModule.max_expr_depth", 5, 1,
                20, "max expression depth");
REDLINE_TUNABLE(kWasmMaxBodySize, "generators.WasmModule.max_body_size", 20, 1,
                200, "max body instructions");
REDLINE_TUNABLE(kWasmMaxLoopCount, "generators.WasmModule.max_loop_count", 16,
                1, 64, "max iterations of a counted Wasm loop");
REDLINE_TUNABLE(kWasmInterestingConstProb,
                "generators.WasmModule.interesting_const_probability", 0.3, 0,
                1, "probability of an edge-case numeric constant");
REDLINE_TUNABLE(kWasmTrapMemoryProb,
                "generators.WasmModule.trap_memory_probability", 0.05, 0, 1,
                "probability of unmasked memory access");
REDLINE_TUNABLE(kWasmTrapDivProb, "generators.WasmModule.trap_div_probability",
                0.05, 0, 1, "probability of non-guarded div/rem");
REDLINE_TUNABLE(kWasmTrapConvertProb,
                "generators.WasmModule.trap_convert_probability", 0.05, 0, 1,
                "probability of non-saturating convert");

// Stage 2 feature tunables (memory64, multi-memory, atomics, GC, SIMD, EH,
// JSPI, tables, tail calls, JS string builtins).
REDLINE_TUNABLE(kWasmMemory64Prob, "generators.WasmModule.memory64_probability",
                0.4, 0, 1, "probability that a memory uses 64-bit addresses");
REDLINE_TUNABLE(kWasmSharedMemoryProb,
                "generators.WasmModule.shared_memory_probability", 0.35, 0, 1,
                "probability that a memory is shared");
REDLINE_TUNABLE(kWasmMultiMemoryProb,
                "generators.WasmModule.multi_memory_probability", 0.4, 0, 1,
                "probability of emitting multiple linear memories");
REDLINE_TUNABLE(kWasmMaxMemories, "generators.WasmModule.max_memories", 3, 1, 4,
                "max linear memories per module");
REDLINE_TUNABLE(kWasmImportedMemoryProb,
                "generators.WasmModule.imported_memory_probability", 0.25, 0, 1,
                "probability that memory 0 is imported from JS");
REDLINE_TUNABLE(kWasmAtomicOpProb,
                "generators.WasmModule.atomic_op_probability", 0.35, 0, 1,
                "probability of choosing an atomic memory operation");
REDLINE_TUNABLE(kWasmBulkMemoryProb,
                "generators.WasmModule.bulk_memory_probability", 0.35, 0, 1,
                "probability of emitting bulk memory operations");
REDLINE_TUNABLE(kWasmGcProb, "generators.WasmModule.gc_probability", 0.7, 0, 1,
                "probability of emitting WasmGC types and instructions");
REDLINE_TUNABLE(kWasmSimdProb, "generators.WasmModule.simd_probability", 0.6, 0,
                1, "probability of emitting Wasm SIMD v128 instructions");
REDLINE_TUNABLE(kWasmEhProb, "generators.WasmModule.eh_probability", 0.55, 0, 1,
                "probability of emitting try_table exception handling");
REDLINE_TUNABLE(kWasmJspiProb, "generators.WasmModule.jspi_probability", 0.3, 0,
                1,
                "probability of emitting JSPI suspending and promising hooks");
REDLINE_TUNABLE(kWasmTable64Prob, "generators.WasmModule.table64_probability",
                0.3, 0, 1, "probability that a table uses 64-bit indices");
REDLINE_TUNABLE(kWasmTailCallProb,
                "generators.WasmModule.tail_call_probability", 0.25, 0, 1,
                "probability of emitting tail-call instructions");
REDLINE_TUNABLE(kWasmJsStringProb,
                "generators.WasmModule.js_string_builtins_probability", 0.3, 0,
                1, "probability of importing wasm:js-string builtins");
REDLINE_TUNABLE(kWasmStartFuncProb,
                "generators.WasmModule.start_function_probability", 0.35, 0, 1,
                "probability of emitting a start section when eligible");
REDLINE_TUNABLE(kWasmGcTypeProb, "generators.WasmModule.gc_type_probability",
                0.35, 0, 1,
                "probability of choosing a WasmGC reference value type");
REDLINE_TUNABLE(kWasmSimdTypeProb,
                "generators.WasmModule.simd_type_probability", 0.25, 0, 1,
                "probability of choosing v128 for locals, globals, and stores");
REDLINE_TUNABLE(kWasmJspiLargeFrameProb,
                "generators.WasmModule.jspi_large_frame_probability", 0.25, 0,
                1, "probability of adding extra i64 locals to the JSPI export");
REDLINE_TUNABLE(kWasmJspiLargeFrameLocals,
                "generators.WasmModule.jspi_large_frame_locals", 64, 0, 128,
                "number of extra i64 locals in a large JSPI frame");

namespace {

constexpr int kMaxLoopNesting = 2;
constexpr uint32_t kSafeMemoryMask = 0x7FF8;
constexpr uint32_t kMaxMemoryExtraPages = 4;

void EncodeU32(std::vector<uint8_t>& buf, uint32_t value) {
  do {
    uint8_t byte = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    buf.push_back(byte);
  } while (value != 0);
}

void EncodeU64(std::vector<uint8_t>& buf, uint64_t value) {
  do {
    uint8_t byte = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    buf.push_back(byte);
  } while (value != 0);
}

void EncodeI32(std::vector<uint8_t>& buf, int32_t value) {
  bool more = true;
  while (more) {
    uint8_t byte = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    bool sign_bit = (byte & 0x40) != 0;
    if ((value == 0 && !sign_bit) || (value == -1 && sign_bit)) {
      more = false;
    } else {
      byte |= 0x80;
    }
    buf.push_back(byte);
  }
}

void EncodeI64(std::vector<uint8_t>& buf, int64_t value) {
  bool more = true;
  while (more) {
    uint8_t byte = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    bool sign_bit = (byte & 0x40) != 0;
    if ((value == 0 && !sign_bit) || (value == -1 && sign_bit)) {
      more = false;
    } else {
      byte |= 0x80;
    }
    buf.push_back(byte);
  }
}

void EncodeS33(std::vector<uint8_t>& buf, uint32_t idx) {
  EncodeI64(buf, static_cast<int64_t>(idx));
}

void EncodeF32(std::vector<uint8_t>& buf, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  for (size_t i = 0; i < 4; ++i) {
    buf.push_back(static_cast<uint8_t>((bits >> (8 * i)) & 0xFF));
  }
}

void EncodeF64(std::vector<uint8_t>& buf, double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  for (size_t i = 0; i < 8; ++i) {
    buf.push_back(static_cast<uint8_t>((bits >> (8 * i)) & 0xFF));
  }
}

void EncodeString(std::vector<uint8_t>& buf, const std::string& str) {
  EncodeU32(buf, static_cast<uint32_t>(str.size()));
  for (char c : str) {
    buf.push_back(static_cast<uint8_t>(c));
  }
}

void EncodeValType(std::vector<uint8_t>& buf, const WasmValType& vt) {
  if (vt.kind == WasmType::kRefNull) {
    buf.push_back(0x63);
    EncodeS33(buf, vt.type_idx);
  } else if (vt.kind == WasmType::kRef) {
    buf.push_back(0x64);
    if (vt.heap_type != 0) {
      buf.push_back(vt.heap_type);
    } else {
      EncodeS33(buf, vt.type_idx);
    }
  } else {
    buf.push_back(static_cast<uint8_t>(vt.kind));
  }
}

void EncodeHeapType(std::vector<uint8_t>& buf, const WasmValType& vt) {
  if (vt.IsConcreteRef()) {
    EncodeS33(buf, vt.type_idx);
  } else if (vt.kind == WasmType::kRef && vt.heap_type != 0) {
    buf.push_back(vt.heap_type);
  } else {
    buf.push_back(static_cast<uint8_t>(vt.kind));
  }
}

void EmitSection(std::vector<uint8_t>& out, uint8_t section_id,
                 const std::vector<uint8_t>& body) {
  out.push_back(section_id);
  EncodeU32(out, static_cast<uint32_t>(body.size()));
  out.insert(out.end(), body.begin(), body.end());
}

void EmitSimdOp(std::vector<uint8_t>& code, uint32_t sub_op) {
  code.push_back(0xFD);
  EncodeU32(code, sub_op);
}

void EmitAtomicOp(std::vector<uint8_t>& code, uint32_t sub_op) {
  code.push_back(0xFE);
  EncodeU32(code, sub_op);
}

void EmitGcOp(std::vector<uint8_t>& code, uint32_t sub_op) {
  code.push_back(0xFB);
  EncodeU32(code, sub_op);
}

void EmitNumericOp(std::vector<uint8_t>& code, uint32_t sub_op) {
  code.push_back(0xFC);
  EncodeU32(code, sub_op);
}

struct StorageType {
  bool packed = false;
  uint8_t packed_code = 0x78;  // `0x78` is i8, `0x77` is i16.
  WasmValType val_type;

  static StorageType PackedI8() { return {true, 0x78, WasmType::kI32}; }
  static StorageType PackedI16() { return {true, 0x77, WasmType::kI32}; }
  static StorageType Value(WasmValType vt) { return {false, 0, vt}; }

  WasmValType Unpacked() const {
    return packed ? WasmValType(WasmType::kI32) : val_type;
  }
  bool IsDefaultable() const { return packed || val_type.IsDefaultable(); }
  bool IsNumericOrPacked() const {
    return packed || val_type.IsNumericOrSimd();
  }
};

void EncodeStorageType(std::vector<uint8_t>& buf, const StorageType& st) {
  if (st.packed) {
    buf.push_back(st.packed_code);
  } else {
    EncodeValType(buf, st.val_type);
  }
}

struct StructField {
  StorageType type;
  bool mut = true;
};

struct StructDef {
  uint32_t type_idx = 0;
  bool is_final = true;
  bool has_super = false;
  uint32_t super_idx = 0;
  bool defaultable = true;
  std::vector<StructField> fields;
};

struct ArrayDef {
  uint32_t type_idx = 0;
  StorageType elem;
  bool mut = true;
  bool defaultable = true;
};

struct TypeDefEntry {
  enum Kind { kFunc, kStruct, kArray };
  Kind kind = kFunc;
  bool is_final = true;
  bool explicit_subtype = false;
  bool has_super = false;
  uint32_t super_idx = 0;
  WasmSignature sig;
  StructDef struct_def;
  ArrayDef array_def;
};

struct RecGroup {
  bool force_rec = false;
  std::vector<TypeDefEntry> defs;
};

struct TableDef {
  WasmValType elem_type = WasmType::kFuncRef;
  bool is_table64 = false;
  uint32_t min_size = 0;
  uint32_t max_size = 0;
  std::string export_name;
};

struct TagDef {
  uint32_t sig_idx = 0;
  WasmSignature sig;
  bool imported = false;
};

struct DataSegDef {
  bool passive = false;
  uint32_t mem_idx = 0;
  std::vector<uint8_t> data;
};

struct BlockLabel {
  bool is_loop = false;
  bool has_result = false;
  WasmValType result = WasmType::kI32;
};

struct FuncState {
  std::vector<WasmValType> user_locals;
  std::vector<BlockLabel> block_stack;
  size_t my_func = 0;
  size_t my_def_idx = 0;
  size_t loop_counter_base = 0;
  int loop_depth = 0;
  int budget = 1;
  bool allow_calls = true;
  bool is_start = false;
};

class WasmBuilder {
 public:
  explicit WasmBuilder(Random& r) : r_(r) {}

  WasmModule Build() {
    WasmModule m;

    const size_t max_imports = static_cast<size_t>(kWasmMaxImports.AsInt());
    const size_t max_funcs = static_cast<size_t>(kWasmMaxFunctions.AsInt());
    const size_t max_globals = static_cast<size_t>(kWasmMaxGlobals.AsInt());

    size_t num_imports = r_.Index(max_imports + 1);
    size_t num_funcs = 1 + r_.Index(max_funcs);
    const bool has_memory = r_.Chance(kWasmMemoryProb());
    m.table = r_.Chance(kWasmTableProb());
    const size_t num_globals = r_.Index(max_globals + 1);

    m.has_gc = r_.Chance(kWasmGcProb());
    m.has_simd = r_.Chance(kWasmSimdProb());
    m.has_eh = r_.Chance(kWasmEhProb());
    m.has_js_string_builtins = m.has_gc && r_.Chance(kWasmJsStringProb());
    has_gc_ = m.has_gc;
    has_simd_ = m.has_simd;
    has_eh_ = m.has_eh;
    has_js_string_ = m.has_js_string_builtins;

    // Reserve at least 3 functions when a table exists so 2 or more callees
    // share one signature while a higher-index function executes
    // `call_indirect` without recursion.
    if (m.table && max_funcs >= 3 && num_funcs < 3) {
      num_funcs = 3;
    }

    // Reserve at least 1 JS import when JSPI is enabled.
    const bool want_jspi =
        max_imports >= 1 && num_funcs >= 1 && r_.Chance(kWasmJspiProb());
    if (want_jspi && num_imports == 0) {
      num_imports = 1;
    }
    if (want_jspi && m.table && max_funcs >= 4 && num_funcs < 4) {
      num_funcs = 4;
    }

    // Build compile-time JS string builtin singleton types first so their
    // canonical recgroups stay singleton as V8 requires.
    if (has_js_string_) {
      SetupJsStringTypes();
    }

    // Build one recursive GC type group with struct inheritance, forward
    // references, packed fields, and numeric/reference arrays.
    if (has_gc_) {
      SetupGcRecGroup(&m);
    }

    // Register JS function imports (`m.f0`, `m.f1`, ...).
    for (size_t i = 0; i < num_imports; ++i) {
      WasmSignature sig = RandomJsSignature(/*allow_refs=*/has_gc_);
      m.imports.push_back(sig);
      import_func_types_.push_back(AddStandaloneFuncType(sig));
      all_func_sigs_.push_back(sig);
      all_func_type_idxs_.push_back(import_func_types_.back());
    }

    // Append `wasm:js-string` function imports right after JS imports.
    if (has_js_string_) {
      RegisterJsStringImports();
    }
    total_imported_funcs_ = all_func_sigs_.size();

    // Put multiple callees with one shared signature at the start of defined
    // functions when `m.table` is true. Keep space after `table_callees_` for
    // `m.indirect_caller` and `m.jspi_export`.
    if (m.table && num_funcs >= 3) {
      const size_t tail_reserve = (want_jspi && num_funcs >= 4) ? 2 : 1;
      const size_t max_callees = std::min<size_t>(num_funcs - tail_reserve, 3);
      table_callees_ = 2 + r_.Index(max_callees - 1);
      callee_sig_ = RandomJsSignature(/*allow_refs=*/has_gc_);
      callee_type_idx_ = AddStandaloneFuncType(callee_sig_);
    } else if (m.table && num_funcs == 2) {
      table_callees_ = 1;
      callee_sig_ = RandomJsSignature(/*allow_refs=*/has_gc_);
      callee_type_idx_ = AddStandaloneFuncType(callee_sig_);
    }

    // Place `m.jspi_export` at the highest defined index `num_funcs - 1` so no
    // other defined function or table callee calls it. Place
    // `m.indirect_caller` at `num_funcs - 2` when both exist.
    const int jspi_def_idx =
        (want_jspi && !m.imports.empty() && num_funcs > table_callees_)
            ? static_cast<int>(num_funcs - 1)
            : -1;
    int indirect_def_idx = -1;
    if (m.table && table_callees_ >= 2 && kWasmMaxParams.AsInt() >= 1) {
      if (jspi_def_idx >= 0) {
        if (num_funcs >= table_callees_ + 2) {
          indirect_def_idx = static_cast<int>(num_funcs - 2);
        }
      } else {
        indirect_def_idx = static_cast<int>(num_funcs - 1);
      }
    }

    for (size_t i = 0; i < num_funcs; ++i) {
      WasmSignature sig;
      uint32_t type_idx = 0;
      if (i < table_callees_) {
        type_idx = callee_type_idx_;
        sig = callee_sig_;
      } else {
        sig = RandomJsSignature(/*allow_refs=*/has_gc_);
        if (static_cast<int>(i) == indirect_def_idx) {
          if (sig.params.empty()) {
            sig.params.push_back(WasmType::kI32);
          } else {
            sig.params[0] = WasmType::kI32;
          }
          m.indirect_caller = indirect_def_idx;
        }
        type_idx = AddStandaloneFuncType(sig);
      }
      m.functions.push_back(sig);
      func_type_idx_.push_back(type_idx);
      all_func_sigs_.push_back(sig);
      all_func_type_idxs_.push_back(type_idx);
    }

    if (jspi_def_idx >= 0) {
      m.jspi_import = 0;
      m.jspi_export = jspi_def_idx;
    }

    // Pick the optional `start` function before generating bodies so its code
    // avoids calls, traps, and uncaught exceptions.
    if (r_.Chance(kWasmStartFuncProb())) {
      for (size_t i = 0; i < num_funcs; ++i) {
        if (static_cast<int>(i) == m.jspi_export) {
          continue;
        }
        if (m.functions[i].params.empty() && m.functions[i].results.empty()) {
          start_func_def_idx_ = static_cast<int>(i);
          break;
        }
      }
    }

    // Configure linear memories (memory32, memory64, shared, multi-memory, and
    // optional imported memory 0).
    if (has_memory) {
      size_t num_mems = 1;
      const size_t max_mems = static_cast<size_t>(kWasmMaxMemories.AsInt());
      if (max_mems >= 2 && r_.Chance(kWasmMultiMemoryProb())) {
        num_mems = 2 + r_.Index(max_mems - 1);
      }
      const bool import_mem0 = r_.Chance(kWasmImportedMemoryProb());
      for (size_t i = 0; i < num_mems; ++i) {
        WasmMemoryInfo mi;
        mi.is_memory64 = r_.Chance(kWasmMemory64Prob());
        mi.is_shared = r_.Chance(kWasmSharedMemoryProb());
        mi.imported = (i == 0 && import_mem0);
        mi.min_pages = static_cast<uint32_t>(
            1 + r_.Index(static_cast<size_t>(kWasmMaxMemoryPages.AsInt())));
        mi.max_pages = mi.min_pages + kMaxMemoryExtraPages;
        mi.export_name = i == 0 ? "mem" : "mem" + std::to_string(i);
        m.memories.push_back(mi);
      }
      m.memory = !m.memories.empty();
    }

    // Configure tables. Table 0 holds `funcref` and can use 64-bit indices.
    // Optional table 1 holds GC references.
    std::vector<uint32_t> table_entries;
    if (m.table) {
      m.table_is_64 = r_.Chance(kWasmTable64Prob());
      const size_t count = table_callees_ > 0 ? table_callees_ : num_funcs;
      for (size_t i = 0; i < count; ++i) {
        table_entries.push_back(
            static_cast<uint32_t>(total_imported_funcs_ + i));
      }
      m.table_size = static_cast<uint32_t>(table_entries.size());
      m.table_callees = static_cast<uint32_t>(table_callees_);
      tables_.push_back({WasmType::kFuncRef, m.table_is_64, m.table_size,
                         m.table_size + 4, "tab"});
      if (has_gc_ && r_.Coin()) {
        WasmValType ref_elem = r_.Coin() ? WasmValType(WasmType::kAnyRef)
                                         : WasmValType::RefNull(s_base_idx_);
        const bool tab1_is_64 = r_.Chance(kWasmTable64Prob());
        tables_.push_back({ref_elem, tab1_is_64, 4, 8, "tab1"});
        m.has_gc_table = true;
        m.gc_table_is_64 = tab1_is_64;
        m.gc_table_size = 4;
      }
    }

    // Configure exception tags (imported `WebAssembly.JSTag`, optional imported
    // JS tag, and defined module tags).
    if (has_eh_) {
      SetupTags(&m);
    }

    // Configure globals. Imported string constants occupy the first global
    // indices when JS string builtins are enabled.
    if (has_js_string_) {
      str_const_global_0_ = static_cast<uint32_t>(globals_.size());
      globals_.push_back(
          {WasmValType::RefAbstract(WasmType::kExternRef), false, true});
      str_const_global_1_ = static_cast<uint32_t>(globals_.size());
      globals_.push_back(
          {WasmValType::RefAbstract(WasmType::kExternRef), false, true});
      num_imported_globals_ = 2;
    }

    for (size_t i = 0; i < num_globals; ++i) {
      WasmValType type = RandomGlobalType();
      bool is_mut = r_.Coin();
      if (num_globals >= 2 && i == 0) {
        is_mut = true;
      } else if (num_globals >= 2 && i == 1) {
        is_mut = false;
      }
      const uint32_t g_idx = static_cast<uint32_t>(globals_.size());
      globals_.push_back({type, is_mut, false});
      if (is_mut && type.IsJsCompatible() && !type.IsConcreteRef() &&
          type.IsDefaultable()) {
        std::string gname = "g" + std::to_string(m.globals.size());
        m.globals.push_back({gname, type});
        exported_global_indices_.push_back({gname, g_idx});
      }
    }

    // Prepare data segments before emitting the DataCount and Code sections.
    if (m.memory) {
      for (size_t mi = 0; mi < m.memories.size(); ++mi) {
        DataSegDef seg;
        seg.passive = false;
        seg.mem_idx = static_cast<uint32_t>(mi);
        seg.data.resize(32);
        for (uint32_t k = 0; k < 32; ++k) {
          seg.data[k] = static_cast<uint8_t>((k * 37 + 0x42 + mi * 13) & 0xFF);
        }
        data_segs_.push_back(std::move(seg));
      }
    }
    if (m.memory || has_gc_) {
      // Passive segment 0 stays alive for `memory.init` and `array.new_data`.
      passive_data_seg_0_ = static_cast<int>(data_segs_.size());
      DataSegDef p0;
      p0.passive = true;
      p0.data.resize(32);
      for (uint32_t k = 0; k < 32; ++k) {
        p0.data[k] = static_cast<uint8_t>((k * 53 + 0x11) & 0xFF);
      }
      data_segs_.push_back(std::move(p0));

      // Passive segment 1 may be dropped by `data.drop`.
      passive_data_seg_1_ = static_cast<int>(data_segs_.size());
      DataSegDef p1;
      p1.passive = true;
      p1.data.resize(16);
      for (uint32_t k = 0; k < 16; ++k) {
        p1.data[k] = static_cast<uint8_t>((k * 19 + 0x77) & 0xFF);
      }
      data_segs_.push_back(std::move(p1));
    }

    // Track whether a passive `funcref` element segment exists.
    if (m.table && !table_entries.empty()) {
      passive_elem_seg_ = 1;  // Active segment is 0, passive segment is 1.
    }

    // Emit sections in canonical order: Type, Import, Func, Table, Mem, Tag.
    // Next emit Global, Export, Start, Elem, DataCount, Code, and Data.
    std::vector<uint8_t>& b = m.bytes;
    b.insert(b.end(), {0x00, 0x61, 0x73, 0x6D});  // Magic header.
    b.insert(b.end(), {0x01, 0x00, 0x00, 0x00});  // Version 1.

    EmitTypeSection(b);
    EmitImportSection(b, m);
    EmitFunctionSection(b, num_funcs);
    EmitTableSection(b);
    EmitMemorySection(b, m);
    EmitTagSection(b);
    EmitGlobalSection(b);
    EmitExportSection(b, m, num_funcs);
    EmitStartSection(b);
    EmitElementSection(b, m, table_entries);
    EmitDataCountSection(b);
    EmitCodeSection(b, m, num_funcs);
    EmitDataSection(b, m);

    return m;
  }

 private:
  struct Global {
    WasmValType type;
    bool mut = false;
    bool imported = false;
  };

  struct JsStringImport {
    std::string name;
    uint32_t sig_idx = 0;
    uint32_t func_idx = 0;
  };

  Random& r_;
  bool has_gc_ = false;
  bool has_simd_ = false;
  bool has_eh_ = false;
  bool has_js_string_ = false;

  std::vector<RecGroup> rec_groups_;
  uint32_t next_type_idx_ = 0;

  // GC type indices when `has_gc_` is true.
  uint32_t s_base_idx_ = 0;
  uint32_t s_sub_idx_ = 0;
  uint32_t s_alt_idx_ = 0;
  uint32_t a_num_idx_ = 0;
  uint32_t a_ref_idx_ = 0;
  uint32_t blk_sig_s_sub_null_ = 0;
  StructDef s_base_def_;
  StructDef s_sub_def_;
  StructDef s_alt_def_;
  ArrayDef a_num_def_;
  ArrayDef a_ref_def_;

  // JS string builtin metadata when `has_js_string_` is true.
  uint32_t str_array_i16_idx_ = 0;
  uint32_t str_sig_i_r_ = 0;
  uint32_t str_sig_e_i_ = 0;
  uint32_t str_sig_i_ri_ = 0;
  uint32_t str_sig_e_rr_ = 0;
  uint32_t str_sig_e_rii_ = 0;
  uint32_t str_sig_i_rr_ = 0;
  uint32_t str_sig_from_a16_ = 0;
  uint32_t str_sig_into_a16_ = 0;
  std::vector<JsStringImport> js_string_imports_;
  uint32_t str_const_global_0_ = 0;
  uint32_t str_const_global_1_ = 0;

  std::vector<uint32_t> import_func_types_;
  std::vector<uint32_t> func_type_idx_;
  std::vector<WasmSignature> all_func_sigs_;
  std::vector<uint32_t> all_func_type_idxs_;
  size_t total_imported_funcs_ = 0;

  std::vector<Global> globals_;
  size_t num_imported_globals_ = 0;
  std::vector<std::pair<std::string, uint32_t>> exported_global_indices_;

  std::vector<TableDef> tables_;
  size_t table_callees_ = 0;
  uint32_t callee_type_idx_ = 0;
  WasmSignature callee_sig_;
  int passive_elem_seg_ = -1;

  // Tags when `has_eh_` is true.
  std::vector<TagDef> tags_;
  int jstag_idx_ = -1;
  int void_tag_idx_ = -1;
  int i32_tag_idx_ = -1;
  int exported_tag_idx_ = -1;

  std::vector<DataSegDef> data_segs_;
  int passive_data_seg_0_ = -1;
  int passive_data_seg_1_ = -1;
  int start_func_def_idx_ = -1;

  uint32_t AddStandaloneFuncType(const WasmSignature& sig) {
    TypeDefEntry entry;
    entry.kind = TypeDefEntry::kFunc;
    entry.sig = sig;
    RecGroup group;
    group.defs.push_back(std::move(entry));
    rec_groups_.push_back(std::move(group));
    return next_type_idx_++;
  }

  uint32_t AddStandaloneArrayType(StorageType elem, bool mut) {
    TypeDefEntry entry;
    entry.kind = TypeDefEntry::kArray;
    entry.array_def.type_idx = next_type_idx_;
    entry.array_def.elem = elem;
    entry.array_def.mut = mut;
    entry.array_def.defaultable = elem.IsDefaultable();
    RecGroup group;
    group.defs.push_back(std::move(entry));
    rec_groups_.push_back(std::move(group));
    return next_type_idx_++;
  }

  WasmValType RandomScalarType() {
    static constexpr WasmType kTypes[] = {WasmType::kI32, WasmType::kI64,
                                          WasmType::kF32, WasmType::kF64};
    return kTypes[r_.Index(4)];
  }

  WasmValType RandomJsValType(bool allow_refs) {
    if (allow_refs && has_gc_ && r_.Chance(kWasmGcTypeProb())) {
      const size_t k = r_.Index(8);
      switch (k) {
        case 0:
          return WasmType::kAnyRef;
        case 1:
          return WasmType::kExternRef;
        case 2:
          return WasmType::kEqRef;
        case 3:
          return WasmType::kI31Ref;
        case 4:
          return WasmType::kStructRef;
        case 5:
          return WasmType::kArrayRef;
        case 6:
          return WasmValType::RefNull(s_base_idx_);
        default:
          return WasmValType::RefNull(a_num_idx_);
      }
    }
    return RandomScalarType();
  }

  WasmValType RandomLocalType() {
    if (has_simd_ && r_.Chance(kWasmSimdTypeProb())) {
      return WasmType::kV128;
    }
    if (has_gc_ && r_.Chance(kWasmGcTypeProb())) {
      const size_t k = r_.Index(has_eh_ ? 11 : 10);
      switch (k) {
        case 0:
          return WasmType::kAnyRef;
        case 1:
          return WasmType::kExternRef;
        case 2:
          return WasmType::kEqRef;
        case 3:
          return WasmType::kI31Ref;
        case 4:
          return WasmType::kStructRef;
        case 5:
          return WasmType::kArrayRef;
        case 6:
          return WasmValType::RefNull(s_base_idx_);
        case 7:
          return WasmValType::RefNull(s_sub_idx_);
        case 8:
          return WasmValType::RefNull(a_num_idx_);
        case 9:
          return WasmValType::RefNull(a_ref_idx_);
        default:
          return WasmType::kExnRef;
      }
    }
    if (!has_gc_ && has_eh_ && r_.Chance(kWasmGcTypeProb())) {
      return WasmType::kExnRef;
    }
    return RandomScalarType();
  }

  WasmValType RandomGlobalType() {
    if (has_simd_ && r_.Chance(kWasmSimdTypeProb())) {
      return WasmType::kV128;
    }
    if (has_gc_ && r_.Chance(kWasmGcTypeProb())) {
      const size_t k = r_.Index(6);
      switch (k) {
        case 0:
          return WasmType::kAnyRef;
        case 1:
          return WasmType::kExternRef;
        case 2:
          return WasmType::kI31Ref;
        case 3:
          return WasmValType::RefNull(s_base_idx_);
        case 4:
          return WasmValType::Ref(s_base_idx_);
        default:
          return WasmValType::RefAbstract(WasmType::kI31Ref);
      }
    }
    return RandomScalarType();
  }

  WasmSignature RandomJsSignature(bool allow_refs) {
    WasmSignature sig;
    const size_t params =
        r_.Index(static_cast<size_t>(kWasmMaxParams.AsInt()) + 1);
    sig.params.reserve(params);
    for (size_t p = 0; p < params; ++p) {
      sig.params.push_back(RandomJsValType(allow_refs));
    }
    if (r_.Coin()) {
      sig.results.push_back(RandomJsValType(allow_refs));
    }
    return sig;
  }

  void SetupJsStringTypes() {
    str_array_i16_idx_ = AddStandaloneArrayType(StorageType::PackedI16(), true);
    const WasmValType kExt = WasmType::kExternRef;
    const WasmValType kRefExt = WasmValType::RefAbstract(WasmType::kExternRef);
    const WasmValType kI32 = WasmType::kI32;
    const WasmValType kRefNullA16 = WasmValType::RefNull(str_array_i16_idx_);

    str_sig_i_r_ = AddStandaloneFuncType({{kExt}, {kI32}});
    str_sig_e_i_ = AddStandaloneFuncType({{kI32}, {kRefExt}});
    str_sig_i_ri_ = AddStandaloneFuncType({{kExt, kI32}, {kI32}});
    str_sig_e_rr_ = AddStandaloneFuncType({{kExt, kExt}, {kRefExt}});
    str_sig_e_rii_ = AddStandaloneFuncType({{kExt, kI32, kI32}, {kRefExt}});
    str_sig_i_rr_ = AddStandaloneFuncType({{kExt, kExt}, {kI32}});
    str_sig_from_a16_ =
        AddStandaloneFuncType({{kRefNullA16, kI32, kI32}, {kRefExt}});
    str_sig_into_a16_ =
        AddStandaloneFuncType({{kExt, kRefNullA16, kI32}, {kI32}});
  }

  void RegisterJsStringImports() {
    auto add = [&](const char* name, uint32_t sig_idx,
                   const WasmSignature& sig) {
      const uint32_t fidx = static_cast<uint32_t>(all_func_sigs_.size());
      js_string_imports_.push_back({name, sig_idx, fidx});
      all_func_sigs_.push_back(sig);
      all_func_type_idxs_.push_back(sig_idx);
    };
    const WasmValType kExt = WasmType::kExternRef;
    const WasmValType kRefExt = WasmValType::RefAbstract(WasmType::kExternRef);
    const WasmValType kI32 = WasmType::kI32;
    const WasmValType kRefNullA16 = WasmValType::RefNull(str_array_i16_idx_);

    add("test", str_sig_i_r_, {{kExt}, {kI32}});
    add("length", str_sig_i_r_, {{kExt}, {kI32}});
    add("fromCharCode", str_sig_e_i_, {{kI32}, {kRefExt}});
    add("fromCodePoint", str_sig_e_i_, {{kI32}, {kRefExt}});
    add("charCodeAt", str_sig_i_ri_, {{kExt, kI32}, {kI32}});
    add("codePointAt", str_sig_i_ri_, {{kExt, kI32}, {kI32}});
    add("concat", str_sig_e_rr_, {{kExt, kExt}, {kRefExt}});
    add("substring", str_sig_e_rii_, {{kExt, kI32, kI32}, {kRefExt}});
    add("equals", str_sig_i_rr_, {{kExt, kExt}, {kI32}});
    add("compare", str_sig_i_rr_, {{kExt, kExt}, {kI32}});
    add("fromCharCodeArray", str_sig_from_a16_,
        {{kRefNullA16, kI32, kI32}, {kRefExt}});
    add("intoCharCodeArray", str_sig_into_a16_,
        {{kExt, kRefNullA16, kI32}, {kI32}});
  }

  void SetupGcRecGroup(WasmModule* m) {
    // Put all GC types in one `0x4E` rec group so forward references stay
    // valid.
    s_base_idx_ = next_type_idx_ + 0;
    s_sub_idx_ = next_type_idx_ + 1;
    s_alt_idx_ = next_type_idx_ + 2;
    a_num_idx_ = next_type_idx_ + 3;
    a_ref_idx_ = next_type_idx_ + 4;
    next_type_idx_ += 5;

    // `$S_base`: extensible base struct with defaultable fields, packed storage
    // types, and a forward reference to `$S_sub` or `$A_num`.
    s_base_def_.type_idx = s_base_idx_;
    s_base_def_.is_final = false;
    s_base_def_.has_super = false;
    s_base_def_.defaultable = true;
    s_base_def_.fields.push_back({StorageType::Value(WasmType::kI32), true});
    s_base_def_.fields.push_back(
        {r_.Coin() ? StorageType::PackedI8() : StorageType::PackedI16(), true});
    if (has_simd_ && r_.Coin()) {
      s_base_def_.fields.push_back({StorageType::Value(WasmType::kV128), true});
    } else {
      s_base_def_.fields.push_back(
          {StorageType::Value(r_.Coin() ? WasmType::kF64 : WasmType::kI64),
           r_.Coin()});
    }
    s_base_def_.fields.push_back(
        {StorageType::Value(r_.Coin() ? WasmValType::RefNull(s_sub_idx_)
                                      : WasmValType::RefNull(a_num_idx_)),
         true});

    // `$S_sub`: subtype of `$S_base` with optional non-nullable field.
    s_sub_def_.type_idx = s_sub_idx_;
    s_sub_def_.is_final = true;
    s_sub_def_.has_super = true;
    s_sub_def_.super_idx = s_base_idx_;
    s_sub_def_.fields = s_base_def_.fields;
    if (r_.Coin()) {
      s_sub_def_.fields.push_back(
          {StorageType::Value(WasmValType::RefAbstract(WasmType::kI31Ref)),
           false});
      s_sub_def_.defaultable = false;
    } else {
      s_sub_def_.fields.push_back(
          {StorageType::Value(WasmType::kAnyRef), true});
      s_sub_def_.defaultable = true;
    }

    // `$S_alt`: sibling subtype of `$S_base` so dynamic casts fail and succeed.
    s_alt_def_.type_idx = s_alt_idx_;
    s_alt_def_.is_final = true;
    s_alt_def_.has_super = true;
    s_alt_def_.super_idx = s_base_idx_;
    s_alt_def_.fields = s_base_def_.fields;
    s_alt_def_.fields.push_back({StorageType::Value(WasmType::kF32), true});
    s_alt_def_.defaultable = true;

    // `$A_num`: mutable numeric or packed array.
    a_num_def_.type_idx = a_num_idx_;
    a_num_def_.mut = true;
    a_num_def_.defaultable = true;
    {
      const size_t k = r_.Index(has_simd_ ? 7 : 6);
      switch (k) {
        case 0:
          a_num_def_.elem = StorageType::PackedI8();
          break;
        case 1:
          a_num_def_.elem = StorageType::PackedI16();
          break;
        case 2:
          a_num_def_.elem = StorageType::Value(WasmType::kI32);
          break;
        case 3:
          a_num_def_.elem = StorageType::Value(WasmType::kI64);
          break;
        case 4:
          a_num_def_.elem = StorageType::Value(WasmType::kF32);
          break;
        case 5:
          a_num_def_.elem = StorageType::Value(WasmType::kF64);
          break;
        default:
          a_num_def_.elem = StorageType::Value(WasmType::kV128);
          break;
      }
    }

    // `$A_ref`: reference array.
    a_ref_def_.type_idx = a_ref_idx_;
    a_ref_def_.mut = r_.Coin();
    {
      const size_t k = r_.Index(5);
      WasmValType et;
      switch (k) {
        case 0:
          et = WasmValType::RefNull(s_base_idx_);
          break;
        case 1:
          et = WasmType::kAnyRef;
          break;
        case 2:
          et = WasmType::kEqRef;
          break;
        case 3:
          et = WasmType::kI31Ref;
          break;
        default:
          et = WasmType::kFuncRef;
          break;
      }
      a_ref_def_.elem = StorageType::Value(et);
      a_ref_def_.defaultable = true;
    }

    RecGroup group;
    group.force_rec = true;
    auto push_struct = [&](const StructDef& sd) {
      TypeDefEntry e;
      e.kind = TypeDefEntry::kStruct;
      e.explicit_subtype = true;
      e.is_final = sd.is_final;
      e.has_super = sd.has_super;
      e.super_idx = sd.super_idx;
      e.struct_def = sd;
      group.defs.push_back(std::move(e));
    };
    auto push_array = [&](const ArrayDef& ad) {
      TypeDefEntry e;
      e.kind = TypeDefEntry::kArray;
      e.array_def = ad;
      group.defs.push_back(std::move(e));
    };
    push_struct(s_base_def_);
    push_struct(s_sub_def_);
    push_struct(s_alt_def_);
    push_array(a_num_def_);
    push_array(a_ref_def_);
    rec_groups_.push_back(std::move(group));

    m->struct_types = {s_base_idx_, s_sub_idx_, s_alt_idx_};
    m->array_types = {a_num_idx_, a_ref_idx_};

    // Pre-register block result signature for concrete ref `br_on_cast`.
    blk_sig_s_sub_null_ =
        AddStandaloneFuncType({{}, {WasmValType::RefNull(s_sub_idx_)}});
  }

  void SetupTags(WasmModule* m) {
    // Optionally import `WebAssembly.JSTag` (`(externref) -> ()`) so Wasm
    // `try_table` catches JS exceptions from throwing imports.
    if (r_.Coin()) {
      WasmSignature js_sig{{WasmType::kExternRef}, {}};
      const uint32_t sig_idx = AddStandaloneFuncType(js_sig);
      jstag_idx_ = static_cast<int>(tags_.size());
      tags_.push_back({sig_idx, js_sig, /*imported=*/true});
      m->has_imported_jstag = true;
    }

    // Optionally import one JS-constructed `WebAssembly.Tag`.
    if (r_.Coin()) {
      WasmSignature tsig;
      const size_t nparams = 1 + r_.Index(2);
      for (size_t i = 0; i < nparams; ++i) {
        tsig.params.push_back(RandomScalarType());
      }
      const uint32_t sig_idx = AddStandaloneFuncType(tsig);
      tags_.push_back({sig_idx, tsig, /*imported=*/true});
      m->imported_tags.push_back(tsig);
    }

    // Define a 0-param tag and a 1-param `i32` tag in Section 13.
    {
      WasmSignature void_sig{{}, {}};
      const uint32_t sig_idx = AddStandaloneFuncType(void_sig);
      void_tag_idx_ = static_cast<int>(tags_.size());
      tags_.push_back({sig_idx, void_sig, /*imported=*/false});
    }
    {
      WasmSignature i32_sig{{WasmType::kI32}, {}};
      const uint32_t sig_idx = AddStandaloneFuncType(i32_sig);
      i32_tag_idx_ = static_cast<int>(tags_.size());
      tags_.push_back({sig_idx, i32_sig, /*imported=*/false});
      exported_tag_idx_ = i32_tag_idx_;
      m->exported_tags.push_back(i32_sig);
    }
  }

  void EmitTypeDef(std::vector<uint8_t>& sec, const TypeDefEntry& td) {
    if (td.explicit_subtype) {
      sec.push_back(td.is_final ? 0x4F : 0x50);
      if (td.has_super) {
        EncodeU32(sec, 1);
        EncodeU32(sec, td.super_idx);
      } else {
        EncodeU32(sec, 0);
      }
    }
    if (td.kind == TypeDefEntry::kFunc) {
      sec.push_back(0x60);
      EncodeU32(sec, static_cast<uint32_t>(td.sig.params.size()));
      for (const WasmValType& t : td.sig.params) {
        EncodeValType(sec, t);
      }
      EncodeU32(sec, static_cast<uint32_t>(td.sig.results.size()));
      for (const WasmValType& t : td.sig.results) {
        EncodeValType(sec, t);
      }
    } else if (td.kind == TypeDefEntry::kStruct) {
      sec.push_back(0x5F);
      EncodeU32(sec, static_cast<uint32_t>(td.struct_def.fields.size()));
      for (const StructField& f : td.struct_def.fields) {
        EncodeStorageType(sec, f.type);
        sec.push_back(f.mut ? 0x01 : 0x00);
      }
    } else {
      sec.push_back(0x5E);
      EncodeStorageType(sec, td.array_def.elem);
      sec.push_back(td.array_def.mut ? 0x01 : 0x00);
    }
  }

  void EmitTypeSection(std::vector<uint8_t>& b) {
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(rec_groups_.size()));
    for (const RecGroup& rg : rec_groups_) {
      if (rg.force_rec || rg.defs.size() > 1) {
        sec.push_back(0x4E);
        EncodeU32(sec, static_cast<uint32_t>(rg.defs.size()));
      }
      for (const TypeDefEntry& td : rg.defs) {
        EmitTypeDef(sec, td);
      }
    }
    EmitSection(b, 1, sec);
  }

  void EmitMemoryLimits(std::vector<uint8_t>& sec, const WasmMemoryInfo& mi) {
    uint8_t flags = 0x01;  // With maximum.
    if (mi.is_shared) {
      flags |= 0x02;
    }
    if (mi.is_memory64) {
      flags |= 0x04;
    }
    sec.push_back(flags);
    if (mi.is_memory64) {
      EncodeU64(sec, mi.min_pages);
      EncodeU64(sec, mi.max_pages);
    } else {
      EncodeU32(sec, mi.min_pages);
      EncodeU32(sec, mi.max_pages);
    }
  }

  void EmitImportSection(std::vector<uint8_t>& b, const WasmModule& m) {
    const size_t num_imp_mems =
        (!m.memories.empty() && m.memories[0].imported) ? 1 : 0;
    size_t num_imp_tags = 0;
    for (const TagDef& td : tags_) {
      if (td.imported) {
        ++num_imp_tags;
      }
    }
    const size_t total = m.imports.size() + js_string_imports_.size() +
                         num_imp_mems + num_imp_tags + num_imported_globals_;
    if (total == 0) {
      return;
    }

    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(total));

    // JS function imports `m.f0..`.
    for (size_t i = 0; i < m.imports.size(); ++i) {
      EncodeString(sec, "m");
      EncodeString(sec, "f" + std::to_string(i));
      sec.push_back(0x00);
      EncodeU32(sec, import_func_types_[i]);
    }

    // Compile-time `wasm:js-string` imports.
    for (const JsStringImport& si : js_string_imports_) {
      EncodeString(sec, "wasm:js-string");
      EncodeString(sec, si.name);
      sec.push_back(0x00);
      EncodeU32(sec, si.sig_idx);
    }

    // Imported memory `m.mem0`.
    if (num_imp_mems > 0) {
      EncodeString(sec, "m");
      EncodeString(sec, "mem0");
      sec.push_back(0x02);
      EmitMemoryLimits(sec, m.memories[0]);
    }

    // Imported tags (`m.jstag` and `m.t0..`).
    size_t js_tag_num = 0;
    for (size_t i = 0; i < tags_.size(); ++i) {
      if (!tags_[i].imported) {
        continue;
      }
      EncodeString(sec, "m");
      if (static_cast<int>(i) == jstag_idx_) {
        EncodeString(sec, "jstag");
      } else {
        EncodeString(sec, "t" + std::to_string(js_tag_num++));
      }
      sec.push_back(0x04);  // Tag import kind.
      sec.push_back(0x00);  // Exception attribute.
      EncodeU32(sec, tags_[i].sig_idx);
    }

    // Imported string constant globals (`"s"."wasm"` and `"s"."redline"`).
    if (has_js_string_) {
      static constexpr const char* kStrConsts[2] = {"wasm", "redline"};
      for (const char* name : kStrConsts) {
        EncodeString(sec, "s");
        EncodeString(sec, name);
        sec.push_back(0x03);  // Global import kind.
        EncodeValType(sec, WasmValType::RefAbstract(WasmType::kExternRef));
        sec.push_back(0x00);  // Immutable.
      }
    }

    EmitSection(b, 2, sec);
  }

  void EmitFunctionSection(std::vector<uint8_t>& b, size_t num_funcs) {
    if (num_funcs == 0) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(num_funcs));
    for (size_t i = 0; i < num_funcs; ++i) {
      EncodeU32(sec, func_type_idx_[i]);
    }
    EmitSection(b, 3, sec);
  }

  void EmitTableSection(std::vector<uint8_t>& b) {
    if (tables_.empty()) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(tables_.size()));
    for (const TableDef& td : tables_) {
      EncodeValType(sec, td.elem_type);
      const uint8_t flags = td.is_table64 ? 0x05 : 0x01;  // With max.
      sec.push_back(flags);
      if (td.is_table64) {
        EncodeU64(sec, td.min_size);
        EncodeU64(sec, td.max_size);
      } else {
        EncodeU32(sec, td.min_size);
        EncodeU32(sec, td.max_size);
      }
    }
    EmitSection(b, 4, sec);
  }

  void EmitMemorySection(std::vector<uint8_t>& b, const WasmModule& m) {
    size_t defined = 0;
    for (const WasmMemoryInfo& mi : m.memories) {
      if (!mi.imported) {
        ++defined;
      }
    }
    if (defined == 0) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(defined));
    for (const WasmMemoryInfo& mi : m.memories) {
      if (!mi.imported) {
        EmitMemoryLimits(sec, mi);
      }
    }
    EmitSection(b, 5, sec);
  }

  void EmitTagSection(std::vector<uint8_t>& b) {
    size_t defined = 0;
    for (const TagDef& td : tags_) {
      if (!td.imported) {
        ++defined;
      }
    }
    if (defined == 0) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(defined));
    for (const TagDef& td : tags_) {
      if (!td.imported) {
        sec.push_back(0x00);  // Exception attribute.
        EncodeU32(sec, td.sig_idx);
      }
    }
    // Section 13 (`Tag`) appears between Memory (5) and Global (6).
    EmitSection(b, 13, sec);
  }

  void EmitGlobalInitExpr(std::vector<uint8_t>& sec, const WasmValType& type) {
    if (type.kind == WasmType::kI32 && r_.Coin()) {
      // Extended constant expression (`i32.add`/`sub`/`mul`).
      EmitConst(sec, WasmType::kI32);
      EmitConst(sec, WasmType::kI32);
      static constexpr uint8_t kOps[3] = {0x6A, 0x6B, 0x6C};
      sec.push_back(kOps[r_.Index(3)]);
      return;
    }
    if (type.kind == WasmType::kI64 && r_.Coin()) {
      EmitConst(sec, WasmType::kI64);
      EmitConst(sec, WasmType::kI64);
      static constexpr uint8_t kOps[3] = {0x7C, 0x7D, 0x7E};
      sec.push_back(kOps[r_.Index(3)]);
      return;
    }
    if (type.kind == WasmType::kRef && type.heap_type == 0 && has_gc_ &&
        type.type_idx == s_base_idx_) {
      EmitGcOp(sec, 0x01);  // `struct.new_default $S_base`.
      EncodeU32(sec, s_base_idx_);
      return;
    }
    if (type.kind == WasmType::kRef &&
        type.heap_type == static_cast<uint8_t>(WasmType::kI31Ref)) {
      EmitConst(sec, WasmType::kI32);
      EmitGcOp(sec, 0x1C);  // `ref.i31`.
      return;
    }
    if (has_gc_ &&
        (type.kind == WasmType::kAnyRef || type.kind == WasmType::kI31Ref) &&
        r_.Coin()) {
      EmitConst(sec, WasmType::kI32);
      EmitGcOp(sec, 0x1C);  // `ref.i31`.
      return;
    }
    EmitConst(sec, type);
  }

  void EmitGlobalSection(std::vector<uint8_t>& b) {
    const size_t defined = globals_.size() - num_imported_globals_;
    if (defined == 0) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(defined));
    for (size_t i = num_imported_globals_; i < globals_.size(); ++i) {
      const Global& g = globals_[i];
      EncodeValType(sec, g.type);
      sec.push_back(g.mut ? 0x01 : 0x00);
      EmitGlobalInitExpr(sec, g.type);
      sec.push_back(0x0B);
    }
    EmitSection(b, 6, sec);
  }

  void EmitExportSection(std::vector<uint8_t>& b, const WasmModule& m,
                         size_t num_funcs) {
    std::vector<uint8_t> sec;
    const uint32_t num_exports = static_cast<uint32_t>(
        num_funcs + m.memories.size() + tables_.size() +
        exported_global_indices_.size() + (exported_tag_idx_ >= 0 ? 1 : 0));
    EncodeU32(sec, num_exports);

    for (size_t i = 0; i < num_funcs; ++i) {
      EncodeString(sec, "e" + std::to_string(i));
      sec.push_back(0x00);
      EncodeU32(sec, static_cast<uint32_t>(total_imported_funcs_ + i));
    }
    for (size_t i = 0; i < m.memories.size(); ++i) {
      EncodeString(sec, m.memories[i].export_name);
      sec.push_back(0x02);
      EncodeU32(sec, static_cast<uint32_t>(i));
    }
    for (size_t i = 0; i < tables_.size(); ++i) {
      EncodeString(sec, tables_[i].export_name);
      sec.push_back(0x01);
      EncodeU32(sec, static_cast<uint32_t>(i));
    }
    for (const auto& [name, idx] : exported_global_indices_) {
      EncodeString(sec, name);
      sec.push_back(0x03);
      EncodeU32(sec, idx);
    }
    if (exported_tag_idx_ >= 0) {
      EncodeString(sec, "tag0");
      sec.push_back(0x04);
      EncodeU32(sec, static_cast<uint32_t>(exported_tag_idx_));
    }
    EmitSection(b, 7, sec);
  }

  void EmitStartSection(std::vector<uint8_t>& b) {
    if (start_func_def_idx_ < 0) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec,
              static_cast<uint32_t>(total_imported_funcs_ +
                                    static_cast<size_t>(start_func_def_idx_)));
    EmitSection(b, 8, sec);
  }

  void EmitElementSection(std::vector<uint8_t>& b, const WasmModule& m,
                          const std::vector<uint32_t>& table_entries) {
    if (!m.table || table_entries.empty()) {
      return;
    }
    std::vector<uint8_t> sec;
    // Emit 2 segments: active segment 0 for table 0, plus passive segment 1 for
    // `table.init` and `array.new_elem`.
    EncodeU32(sec, 2);

    // Segment 0: active segment for table 0 at offset 0.
    sec.push_back(0x00);
    if (m.table_is_64) {
      sec.push_back(0x42);  // `i64.const 0`.
      EncodeI64(sec, 0);
    } else {
      sec.push_back(0x41);  // `i32.const 0`.
      EncodeI32(sec, 0);
    }
    sec.push_back(0x0B);
    EncodeU32(sec, static_cast<uint32_t>(table_entries.size()));
    for (uint32_t fn_idx : table_entries) {
      EncodeU32(sec, fn_idx);
    }

    // Segment 1: passive `funcref` segment with at least 4 entries so
    // `array.new_elem` always creates a 4-element array.
    const size_t passive_len = std::max<size_t>(4, table_entries.size());
    sec.push_back(0x01);  // Passive segment with function indices.
    sec.push_back(0x00);  // Element kind: function.
    EncodeU32(sec, static_cast<uint32_t>(passive_len));
    for (size_t i = 0; i < passive_len; ++i) {
      EncodeU32(sec, table_entries[i % table_entries.size()]);
    }

    EmitSection(b, 9, sec);
  }

  void EmitDataCountSection(std::vector<uint8_t>& b) {
    if (data_segs_.empty()) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(data_segs_.size()));
    // Section 12 (`DataCount`) appears between Element (9) and Code (10).
    EmitSection(b, 12, sec);
  }

  void EmitCodeSection(std::vector<uint8_t>& b, const WasmModule& m,
                       size_t num_funcs) {
    if (num_funcs == 0) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(num_funcs));
    for (size_t i = 0; i < num_funcs; ++i) {
      std::vector<uint8_t> code = BuildFunctionBody(i, m);
      EncodeU32(sec, static_cast<uint32_t>(code.size()));
      sec.insert(sec.end(), code.begin(), code.end());
    }
    EmitSection(b, 10, sec);
  }

  void EmitDataSection(std::vector<uint8_t>& b, const WasmModule& m) {
    if (data_segs_.empty()) {
      return;
    }
    std::vector<uint8_t> sec;
    EncodeU32(sec, static_cast<uint32_t>(data_segs_.size()));
    for (const DataSegDef& seg : data_segs_) {
      if (seg.passive) {
        sec.push_back(0x01);  // Passive segment.
      } else {
        if (seg.mem_idx == 0) {
          sec.push_back(0x00);  // Active segment for memory 0.
        } else {
          sec.push_back(0x02);  // Active segment with explicit memory index.
          EncodeU32(sec, seg.mem_idx);
        }
        if (m.memories[seg.mem_idx].is_memory64) {
          sec.push_back(0x42);  // `i64.const 0`.
          EncodeI64(sec, 0);
        } else {
          sec.push_back(0x41);  // `i32.const 0`.
          EncodeI32(sec, 0);
        }
        sec.push_back(0x0B);
      }
      EncodeU32(sec, static_cast<uint32_t>(seg.data.size()));
      sec.insert(sec.end(), seg.data.begin(), seg.data.end());
    }
    EmitSection(b, 11, sec);
  }

  void EmitConst(std::vector<uint8_t>& code, const WasmValType& type) {
    if (type.IsRef()) {
      if (type.kind == WasmType::kRef) {
        // Non-nullable reference value.
        if (type.heap_type == static_cast<uint8_t>(WasmType::kI31Ref) ||
            type.heap_type == static_cast<uint8_t>(WasmType::kEqRef) ||
            type.heap_type == static_cast<uint8_t>(WasmType::kAnyRef)) {
          EmitConst(code, WasmType::kI32);
          EmitGcOp(code, 0x1C);  // `ref.i31`.
          return;
        }
        if (type.heap_type == static_cast<uint8_t>(WasmType::kExternRef)) {
          if (has_js_string_) {
            code.push_back(0x23);  // `global.get` string constant.
            EncodeU32(code,
                      r_.Coin() ? str_const_global_0_ : str_const_global_1_);
            return;
          }
          EmitConst(code, WasmType::kI32);
          EmitGcOp(code, 0x1C);  // `ref.i31`.
          EmitGcOp(code, 0x1B);  // `extern.convert_any`.
          return;
        }
        if (type.heap_type == static_cast<uint8_t>(WasmType::kStructRef) &&
            has_gc_) {
          EmitGcOp(code, 0x01);  // `struct.new_default $S_base`.
          EncodeU32(code, s_base_idx_);
          return;
        }
        if (type.heap_type == static_cast<uint8_t>(WasmType::kArrayRef) &&
            has_gc_) {
          code.push_back(0x41);
          EncodeI32(code, 4);
          EmitGcOp(code, 0x07);  // `array.new_default $A_num`.
          EncodeU32(code, a_num_idx_);
          return;
        }
        if (type.heap_type == static_cast<uint8_t>(WasmType::kFuncRef)) {
          code.push_back(0xD2);  // `ref.func` first defined function.
          EncodeU32(code, static_cast<uint32_t>(total_imported_funcs_));
          return;
        }
        if (type.heap_type == 0 && has_gc_) {
          if (type.type_idx == s_base_idx_) {
            EmitGcOp(code, 0x01);
            EncodeU32(code, s_base_idx_);
            return;
          }
          if (type.type_idx == s_alt_idx_) {
            EmitGcOp(code, 0x01);
            EncodeU32(code, s_alt_idx_);
            return;
          }
          if (type.type_idx == s_sub_idx_) {
            for (const StructField& f : s_sub_def_.fields) {
              EmitConst(code, f.type.Unpacked());
            }
            EmitGcOp(code, 0x00);
            EncodeU32(code, s_sub_idx_);
            return;
          }
          if (type.type_idx == a_num_idx_ || type.type_idx == a_ref_idx_ ||
              (has_js_string_ && type.type_idx == str_array_i16_idx_)) {
            code.push_back(0x41);
            EncodeI32(code, 4);
            EmitGcOp(code, 0x07);
            EncodeU32(code, type.type_idx);
            return;
          }
        }
      }
      // Nullable reference fallback: `ref.null <ht>`.
      code.push_back(0xD0);
      EncodeHeapType(code, type);
      return;
    }

    if (type.kind == WasmType::kV128) {
      EmitSimdOp(code, 0x0C);  // `v128.const`.
      const size_t pattern = r_.Index(5);
      if (pattern == 0) {
        for (int i = 0; i < 16; ++i) {
          code.push_back(0x00);
        }
      } else if (pattern == 1) {
        for (int i = 0; i < 16; ++i) {
          code.push_back(0xFF);
        }
      } else if (pattern == 2) {
        static constexpr uint8_t kEdgeBytes[8] = {0x00, 0x01, 0x7F, 0x80,
                                                  0xFE, 0xFF, 0x3F, 0x40};
        for (int i = 0; i < 16; ++i) {
          code.push_back(kEdgeBytes[r_.Index(8)]);
        }
      } else if (pattern == 3) {
        // Four float32 lanes.
        for (int i = 0; i < 4; ++i) {
          EmitFloat32ConstBytes(code);
        }
      } else {
        for (int i = 0; i < 16; ++i) {
          code.push_back(static_cast<uint8_t>(r_.Index(256)));
        }
      }
      return;
    }

    const bool edge = r_.Chance(kWasmInterestingConstProb());
    if (type.kind == WasmType::kI32) {
      static constexpr int32_t kEdges[] = {
          0,  1,  -1,  2,     7,          16,
          31, 32, 255, 65535, 0x7FFFFFFF, std::numeric_limits<int32_t>::min()};
      const int32_t val =
          edge ? kEdges[r_.Index(sizeof(kEdges) / sizeof(kEdges[0]))]
               : static_cast<int32_t>(r_.Range(-1000, 1000));
      code.push_back(0x41);
      EncodeI32(code, val);
    } else if (type.kind == WasmType::kI64) {
      static constexpr int64_t kEdges[] = {0,
                                           1,
                                           -1,
                                           32,
                                           63,
                                           64,
                                           0x7FFFFFFFLL,
                                           -2147483648LL,
                                           0xFFFFFFFFLL,
                                           0x100000000LL,
                                           std::numeric_limits<int64_t>::max(),
                                           std::numeric_limits<int64_t>::min()};
      const int64_t val =
          edge ? kEdges[r_.Index(sizeof(kEdges) / sizeof(kEdges[0]))]
               : r_.Range(-1000, 1000);
      code.push_back(0x42);
      EncodeI64(code, val);
    } else if (type.kind == WasmType::kF32) {
      code.push_back(0x43);
      EmitFloat32ConstBytes(code);
    } else {
      static constexpr double kEdges[] = {
          0.0,
          -0.0,
          1.0,
          -1.0,
          0.5,
          9007199254740992.0,
          -9007199254740992.0,
          std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN()};
      const double val =
          edge ? kEdges[r_.Index(sizeof(kEdges) / sizeof(kEdges[0]))]
               : r_.Double() * 100.0;
      code.push_back(0x44);
      EncodeF64(code, val);
    }
  }

  void EmitFloat32ConstBytes(std::vector<uint8_t>& code) {
    static constexpr float kEdges[] = {0.0f,
                                       -0.0f,
                                       1.0f,
                                       -1.0f,
                                       0.5f,
                                       1e20f,
                                       -1e20f,
                                       std::numeric_limits<float>::infinity(),
                                       -std::numeric_limits<float>::infinity(),
                                       std::numeric_limits<float>::quiet_NaN()};
    const float val = r_.Chance(kWasmInterestingConstProb())
                          ? kEdges[r_.Index(sizeof(kEdges) / sizeof(kEdges[0]))]
                          : static_cast<float>(r_.Double() * 100.0);
    EncodeF32(code, val);
  }

  std::vector<uint8_t> BuildFunctionBody(size_t def_idx, const WasmModule& m) {
    std::vector<uint8_t> code;
    const WasmSignature& sig = m.functions[def_idx];
    const size_t extra_locals =
        r_.Index(static_cast<size_t>(kWasmMaxLocals.AsInt()) + 1);

    std::vector<WasmValType> local_decls;
    local_decls.reserve(extra_locals + 2);
    for (size_t l = 0; l < extra_locals; ++l) {
      local_decls.push_back(RandomLocalType());
    }
    // Seed at least one `v128` local when SIMD is enabled and one GC struct
    // local when GC is enabled so operations reuse live values.
    if (has_simd_ && kWasmMaxLocals.AsInt() >= 1) {
      local_decls.push_back(WasmType::kV128);
    }
    if (has_gc_ && kWasmMaxLocals.AsInt() >= 1) {
      local_decls.push_back(WasmValType::RefNull(s_base_idx_));
    }

    // Reserve dedicated `i32` countdown locals after user locals so nested
    // loops never overwrite each other's counters.
    const int jspi_extra_locals = kWasmJspiLargeFrameLocals.AsInt();
    const bool large_jspi_frame = static_cast<int>(def_idx) == m.jspi_export &&
                                  kWasmMaxLocals.AsInt() >= 1 &&
                                  jspi_extra_locals >= 1 &&
                                  r_.Chance(kWasmJspiLargeFrameProb());
    const uint32_t decl_groups = static_cast<uint32_t>(
        local_decls.size() + 1 + (large_jspi_frame ? 1 : 0));
    EncodeU32(code, decl_groups);
    for (const WasmValType& t : local_decls) {
      EncodeU32(code, 1);
      EncodeValType(code, t);
    }
    EncodeU32(code, static_cast<uint32_t>(kMaxLoopNesting));
    code.push_back(static_cast<uint8_t>(WasmType::kI32));
    if (large_jspi_frame) {
      // Extra `i64` locals exercise JSPI stack switching on larger frames.
      EncodeU32(code, static_cast<uint32_t>(jspi_extra_locals));
      code.push_back(static_cast<uint8_t>(WasmType::kI64));
    }

    FuncState fs;
    fs.user_locals = sig.params;
    fs.user_locals.insert(fs.user_locals.end(), local_decls.begin(),
                          local_decls.end());
    fs.my_func = total_imported_funcs_ + def_idx;
    fs.my_def_idx = def_idx;
    fs.loop_counter_base = fs.user_locals.size();
    fs.is_start = (static_cast<int>(def_idx) == start_func_def_idx_);
    if (fs.is_start) {
      fs.allow_calls = false;
    }

    const bool has_result = !sig.results.empty();
    const WasmValType res =
        has_result ? sig.results[0] : WasmValType(WasmType::kI32);
    fs.block_stack.push_back({/*is_loop=*/false, has_result, res});

    const int max_body = kWasmMaxBodySize.AsInt();
    const int min_body = std::max(1, max_body / 2);
    fs.budget = min_body + static_cast<int>(r_.Index(
                               static_cast<size_t>(max_body - min_body + 1)));

    const int max_depth = kWasmMaxExprDepth.AsInt();

    // Emit one `call_indirect` using parameter 0 in the designated indirect
    // caller so speculative inlining and Wasm deopts trigger from JS.
    if (static_cast<int>(def_idx) == m.indirect_caller && table_callees_ >= 2 &&
        max_body >= 2) {
      fs.budget = std::max(fs.budget, 2);
      --fs.budget;
      EmitCallIndirectStmt(code, fs, max_depth, m, /*use_param0=*/true);
    }

    // Call the JSPI suspending import `f0` in the designated promising export.
    if (static_cast<int>(def_idx) == m.jspi_export && m.jspi_import == 0 &&
        !m.imports.empty() && max_body >= 2) {
      fs.budget = std::max(fs.budget, 2);
      --fs.budget;
      const WasmSignature& isig = m.imports[0];
      for (const WasmValType& p : isig.params) {
        BuildExpr(code, p, fs, max_depth - 1, m);
      }
      code.push_back(0x10);  // `call 0`.
      EncodeU32(code, 0);
      if (!isig.results.empty()) {
        ConsumeResult(code, isig.results[0], fs);
      }
    }

    while (fs.budget > 1) {
      BuildStmt(code, fs, max_depth, m);
    }

    if (has_result) {
      BuildExpr(code, res, fs, max_depth, m);
    }
    code.push_back(0x0B);  // End of function body.
    return code;
  }

  uint32_t PickMemoryIndex(const WasmModule& m, bool prefer_shared = false) {
    if (prefer_shared) {
      std::vector<uint32_t> shared;
      for (size_t i = 0; i < m.memories.size(); ++i) {
        if (m.memories[i].is_shared) {
          shared.push_back(static_cast<uint32_t>(i));
        }
      }
      if (!shared.empty()) {
        return r_.Pick(shared);
      }
    }
    return static_cast<uint32_t>(r_.Index(m.memories.size()));
  }

  void EmitSafeAddress(std::vector<uint8_t>& code, uint32_t mem_idx,
                       FuncState& fs, int depth, const WasmModule& m,
                       bool force_aligned) {
    const bool is64 = m.memories[mem_idx].is_memory64;
    if (is64) {
      BuildExpr(code, WasmType::kI64, fs, depth - 1, m);
      if (force_aligned || fs.is_start || !r_.Chance(kWasmTrapMemoryProb())) {
        code.push_back(0x42);
        EncodeI64(code, static_cast<int64_t>(kSafeMemoryMask));
        code.push_back(0x83);  // `i64.and`.
      }
    } else {
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      if (force_aligned || fs.is_start || !r_.Chance(kWasmTrapMemoryProb())) {
        code.push_back(0x41);
        EncodeI32(code, static_cast<int32_t>(kSafeMemoryMask));
        code.push_back(0x71);  // `i32.and`.
      }
    }
  }

  uint64_t PickMemOffset(bool align8) {
    if (align8) {
      static constexpr uint64_t kAligned[4] = {0, 8, 16, 24};
      return kAligned[r_.Index(4)];
    }
    static constexpr uint64_t kOffsets[4] = {0, 4, 8, 16};
    return kOffsets[r_.Index(4)];
  }

  void EmitMemArg(std::vector<uint8_t>& code, uint32_t mem_idx, uint32_t align,
                  uint64_t offset) {
    if (mem_idx == 0) {
      EncodeU32(code, align);
    } else {
      EncodeU32(code, align | 0x40);
      EncodeU32(code, mem_idx);
    }
    EncodeU64(code, offset);
  }

  void EmitStoreStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                     const WasmModule& m) {
    const uint32_t mem_idx = PickMemoryIndex(m);

    // Optional atomic store (`i32.atomic.store*` / `i64.atomic.store*`).
    if (r_.Chance(kWasmAtomicOpProb())) {
      const uint32_t amem = PickMemoryIndex(m, /*prefer_shared=*/true);
      EmitSafeAddress(code, amem, fs, depth, m, /*force_aligned=*/true);
      if (r_.Coin()) {
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        static constexpr uint32_t kOps[3] = {0x17, 0x19, 0x1A};
        static constexpr uint32_t kAligns[3] = {2, 0, 1};
        const size_t k = r_.Index(3);
        EmitAtomicOp(code, kOps[k]);
        EmitMemArg(code, amem, kAligns[k], PickMemOffset(/*align8=*/true));
      } else {
        BuildExpr(code, WasmType::kI64, fs, depth - 1, m);
        static constexpr uint32_t kOps[4] = {0x18, 0x1B, 0x1C, 0x1D};
        static constexpr uint32_t kAligns[4] = {3, 0, 1, 2};
        const size_t k = r_.Index(4);
        EmitAtomicOp(code, kOps[k]);
        EmitMemArg(code, amem, kAligns[k], PickMemOffset(/*align8=*/true));
      }
      return;
    }

    // Optional SIMD `v128.store` or `v128.store{8,16,32,64}_lane`.
    if (has_simd_ && r_.Chance(kWasmSimdTypeProb())) {
      EmitSafeAddress(code, mem_idx, fs, depth, m, /*force_aligned=*/false);
      BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
      if (r_.Coin()) {
        EmitSimdOp(code, 0x0B);  // `v128.store`.
        EmitMemArg(code, mem_idx, static_cast<uint32_t>(r_.Index(5)),
                   PickMemOffset(/*align8=*/false));
      } else {
        static constexpr uint32_t kOps[4] = {0x58, 0x59, 0x5A, 0x5B};
        static constexpr uint32_t kAligns[4] = {0, 1, 2, 3};
        static constexpr uint8_t kLanes[4] = {16, 8, 4, 2};
        const size_t k = r_.Index(4);
        EmitSimdOp(code, kOps[k]);
        EmitMemArg(code, mem_idx,
                   static_cast<uint32_t>(r_.Index(kAligns[k] + 1)),
                   PickMemOffset(/*align8=*/false));
        code.push_back(static_cast<uint8_t>(r_.Index(kLanes[k])));
      }
      return;
    }

    const WasmValType type = RandomScalarType();
    EmitSafeAddress(code, mem_idx, fs, depth, m, /*force_aligned=*/false);
    BuildExpr(code, type, fs, depth - 1, m);
    uint8_t opcode = 0x36;
    uint32_t max_align = 2;
    if (type.kind == WasmType::kI32) {
      const size_t k = r_.Index(3);
      if (k == 0) {
        opcode = 0x36;
        max_align = 2;
      } else if (k == 1) {
        opcode = 0x3A;
        max_align = 0;
      } else {
        opcode = 0x3B;
        max_align = 1;
      }
    } else if (type.kind == WasmType::kI64) {
      const size_t k = r_.Index(4);
      if (k == 0) {
        opcode = 0x37;
        max_align = 3;
      } else if (k == 1) {
        opcode = 0x3C;
        max_align = 0;
      } else if (k == 2) {
        opcode = 0x3D;
        max_align = 1;
      } else {
        opcode = 0x3E;
        max_align = 2;
      }
    } else if (type.kind == WasmType::kF32) {
      opcode = 0x38;
      max_align = 2;
    } else {
      opcode = 0x39;
      max_align = 3;
    }
    code.push_back(opcode);
    EmitMemArg(code, mem_idx, static_cast<uint32_t>(r_.Index(max_align + 1)),
               PickMemOffset(/*align8=*/false));
  }

  void EmitBulkMemoryStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                          const WasmModule& m) {
    const uint32_t dst_mem = PickMemoryIndex(m);
    const bool dst64 = m.memories[dst_mem].is_memory64;
    const size_t k = r_.Index(5);
    if (k == 0) {
      // `memory.fill` with bounded destination and length.
      EmitSafeAddress(code, dst_mem, fs, depth, m, /*force_aligned=*/false);
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      if (dst64) {
        code.push_back(0x42);
        EncodeI64(code, static_cast<int64_t>(1 + r_.Index(32)));
      } else {
        code.push_back(0x41);
        EncodeI32(code, static_cast<int32_t>(1 + r_.Index(32)));
      }
      EmitNumericOp(code, 11);  // `memory.fill`.
      EncodeU32(code, dst_mem);
    } else if (k == 1) {
      // `memory.copy` within one memory or across two memories.
      const uint32_t src_mem = PickMemoryIndex(m);
      const bool src64 = m.memories[src_mem].is_memory64;
      EmitSafeAddress(code, dst_mem, fs, depth, m, /*force_aligned=*/false);
      EmitSafeAddress(code, src_mem, fs, depth, m, /*force_aligned=*/false);
      if (dst64 && src64) {
        code.push_back(0x42);
        EncodeI64(code, static_cast<int64_t>(1 + r_.Index(32)));
      } else {
        code.push_back(0x41);
        EncodeI32(code, static_cast<int32_t>(1 + r_.Index(32)));
      }
      EmitNumericOp(code, 10);  // `memory.copy`.
      EncodeU32(code, dst_mem);
      EncodeU32(code, src_mem);
    } else if (k == 2 && passive_data_seg_0_ >= 0) {
      // `memory.init` from passive segment 0 (32 bytes).
      EmitSafeAddress(code, dst_mem, fs, depth, m, /*force_aligned=*/false);
      code.push_back(0x41);
      EncodeI32(code, static_cast<int32_t>(r_.Index(16)));
      code.push_back(0x41);
      EncodeI32(code, static_cast<int32_t>(1 + r_.Index(16)));
      EmitNumericOp(code, 8);  // `memory.init`.
      EncodeU32(code, static_cast<uint32_t>(passive_data_seg_0_));
      EncodeU32(code, dst_mem);
    } else if (k == 3) {
      // `memory.grow` by 0 or 1 page, bounded by memory max.
      if (dst64) {
        code.push_back(0x42);
        EncodeI64(code, static_cast<int64_t>(r_.Index(2)));
      } else {
        code.push_back(0x41);
        EncodeI32(code, static_cast<int32_t>(r_.Index(2)));
      }
      code.push_back(0x40);  // `memory.grow`.
      EncodeU32(code, dst_mem);
      ConsumeResult(code, dst64 ? WasmType::kI64 : WasmType::kI32, fs);
    } else if (passive_data_seg_1_ >= 0 && r_.Coin()) {
      EmitNumericOp(code, 9);  // `data.drop` on droppable passive segment 1.
      EncodeU32(code, static_cast<uint32_t>(passive_data_seg_1_));
    } else {
      EmitAtomicOp(code, 0x03);  // `atomic.fence`.
      code.push_back(0x00);
    }
  }

  void EmitAtomicLoadOrRmwExpr(std::vector<uint8_t>& code,
                               const WasmValType& res, FuncState& fs, int depth,
                               const WasmModule& m) {
    const uint32_t mem_idx = PickMemoryIndex(m, /*prefer_shared=*/true);
    const bool is_shared = m.memories[mem_idx].is_shared;
    EmitSafeAddress(code, mem_idx, fs, depth, m, /*force_aligned=*/true);

    if (res.kind == WasmType::kI32) {
      const size_t kind = r_.Index(is_shared ? 5 : 3);
      if (kind == 0) {
        // Atomic load.
        static constexpr uint32_t kOps[3] = {0x10, 0x12, 0x13};
        static constexpr uint32_t kAligns[3] = {2, 0, 1};
        const size_t k = r_.Index(3);
        EmitAtomicOp(code, kOps[k]);
        EmitMemArg(code, mem_idx, kAligns[k], PickMemOffset(/*align8=*/true));
      } else if (kind == 1) {
        // Atomic RMW (`add`, `sub`, `and`, `or`, `xor`, `xchg`).
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        static constexpr uint32_t kBase[6] = {0x1E, 0x25, 0x2C,
                                              0x33, 0x3A, 0x41};
        const size_t op = r_.Index(6);
        const size_t width = r_.Index(3);
        uint32_t sub_op = kBase[op];
        uint32_t align = 2;
        if (width == 1) {
          sub_op += 2;  // 8-bit variant.
          align = 0;
        } else if (width == 2) {
          sub_op += 3;  // 16-bit variant.
          align = 1;
        }
        EmitAtomicOp(code, sub_op);
        EmitMemArg(code, mem_idx, align, PickMemOffset(/*align8=*/true));
      } else if (kind == 2) {
        // Atomic `cmpxchg`.
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        static constexpr uint32_t kOps[3] = {0x48, 0x4A, 0x4B};
        static constexpr uint32_t kAligns[3] = {2, 0, 1};
        const size_t k = r_.Index(3);
        EmitAtomicOp(code, kOps[k]);
        EmitMemArg(code, mem_idx, kAligns[k], PickMemOffset(/*align8=*/true));
      } else if (kind == 3) {
        // `memory.atomic.notify`.
        code.push_back(0x41);
        EncodeI32(code, 1);
        EmitAtomicOp(code, 0x00);
        EmitMemArg(code, mem_idx, 2, PickMemOffset(/*align8=*/true));
      } else {
        // `memory.atomic.wait32` / `wait64` on shared memory with timeout 0.
        if (r_.Coin()) {
          BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
          code.push_back(0x42);
          EncodeI64(code, 0);
          EmitAtomicOp(code, 0x01);  // `wait32`.
          EmitMemArg(code, mem_idx, 2, PickMemOffset(/*align8=*/true));
        } else {
          BuildExpr(code, WasmType::kI64, fs, depth - 1, m);
          code.push_back(0x42);
          EncodeI64(code, 0);
          EmitAtomicOp(code, 0x02);  // `wait64`.
          EmitMemArg(code, mem_idx, 3, PickMemOffset(/*align8=*/true));
        }
      }
      return;
    }

    // `i64` atomic load, RMW, or cmpxchg.
    const size_t kind = r_.Index(3);
    if (kind == 0) {
      static constexpr uint32_t kOps[4] = {0x11, 0x14, 0x15, 0x16};
      static constexpr uint32_t kAligns[4] = {3, 0, 1, 2};
      const size_t k = r_.Index(4);
      EmitAtomicOp(code, kOps[k]);
      EmitMemArg(code, mem_idx, kAligns[k], PickMemOffset(/*align8=*/true));
    } else if (kind == 1) {
      BuildExpr(code, WasmType::kI64, fs, depth - 1, m);
      static constexpr uint32_t kBase[6] = {0x1F, 0x26, 0x2D, 0x34, 0x3B, 0x42};
      const size_t op = r_.Index(6);
      const size_t width = r_.Index(4);
      uint32_t sub_op = kBase[op];
      uint32_t align = 3;
      if (width == 1) {
        sub_op += 3;  // 8-bit variant.
        align = 0;
      } else if (width == 2) {
        sub_op += 4;  // 16-bit variant.
        align = 1;
      } else if (width == 3) {
        sub_op += 5;  // 32-bit variant.
        align = 2;
      }
      EmitAtomicOp(code, sub_op);
      EmitMemArg(code, mem_idx, align, PickMemOffset(/*align8=*/true));
    } else {
      BuildExpr(code, WasmType::kI64, fs, depth - 1, m);
      BuildExpr(code, WasmType::kI64, fs, depth - 1, m);
      static constexpr uint32_t kOps[4] = {0x49, 0x4C, 0x4D, 0x4E};
      static constexpr uint32_t kAligns[4] = {3, 0, 1, 2};
      const size_t k = r_.Index(4);
      EmitAtomicOp(code, kOps[k]);
      EmitMemArg(code, mem_idx, kAligns[k], PickMemOffset(/*align8=*/true));
    }
  }

  void EmitLoadExpr(std::vector<uint8_t>& code, const WasmValType& res,
                    FuncState& fs, int depth, const WasmModule& m) {
    // `memory.size` matching 32-bit or 64-bit memory address type.
    if ((res.kind == WasmType::kI32 || res.kind == WasmType::kI64) &&
        r_.Chance(0.15)) {
      std::vector<uint32_t> matching;
      for (size_t i = 0; i < m.memories.size(); ++i) {
        const bool want64 = (res.kind == WasmType::kI64);
        if (m.memories[i].is_memory64 == want64) {
          matching.push_back(static_cast<uint32_t>(i));
        }
      }
      if (!matching.empty()) {
        code.push_back(0x3F);  // `memory.size`.
        EncodeU32(code, r_.Pick(matching));
        return;
      }
    }

    if ((res.kind == WasmType::kI32 || res.kind == WasmType::kI64) &&
        r_.Chance(kWasmAtomicOpProb())) {
      EmitAtomicLoadOrRmwExpr(code, res, fs, depth, m);
      return;
    }

    const uint32_t mem_idx = PickMemoryIndex(m);
    EmitSafeAddress(code, mem_idx, fs, depth, m, /*force_aligned=*/false);
    uint8_t opcode = 0x28;
    uint32_t max_align = 2;
    if (res.kind == WasmType::kI32) {
      static constexpr uint8_t kOps[] = {0x28, 0x2C, 0x2D, 0x2E, 0x2F};
      static constexpr uint32_t kAligns[] = {2, 0, 0, 1, 1};
      const size_t k = r_.Index(5);
      opcode = kOps[k];
      max_align = kAligns[k];
    } else if (res.kind == WasmType::kI64) {
      static constexpr uint8_t kOps[] = {0x29, 0x30, 0x31, 0x32,
                                         0x33, 0x34, 0x35};
      static constexpr uint32_t kAligns[] = {3, 0, 0, 1, 1, 2, 2};
      const size_t k = r_.Index(7);
      opcode = kOps[k];
      max_align = kAligns[k];
    } else if (res.kind == WasmType::kF32) {
      opcode = 0x2A;
      max_align = 2;
    } else {
      opcode = 0x2B;
      max_align = 3;
    }
    code.push_back(opcode);
    EmitMemArg(code, mem_idx, static_cast<uint32_t>(r_.Index(max_align + 1)),
               PickMemOffset(/*align8=*/false));
  }

  void ConsumeResult(std::vector<uint8_t>& code, const WasmValType& type,
                     const FuncState& fs) {
    if (r_.Coin()) {
      std::vector<size_t> cands;
      for (size_t i = 0; i < fs.user_locals.size(); ++i) {
        if (fs.user_locals[i] == type) {
          cands.push_back(i);
        }
      }
      if (!cands.empty()) {
        code.push_back(0x21);  // `local.set`.
        EncodeU32(code, static_cast<uint32_t>(r_.Pick(cands)));
        return;
      }
    }
    if (r_.Coin()) {
      std::vector<size_t> cands;
      for (size_t i = 0; i < globals_.size(); ++i) {
        if (globals_[i].mut && globals_[i].type == type) {
          cands.push_back(i);
        }
      }
      if (!cands.empty()) {
        code.push_back(0x24);  // `global.set`.
        EncodeU32(code, static_cast<uint32_t>(r_.Pick(cands)));
        return;
      }
    }
    code.push_back(0x1A);  // `drop`.
  }

  void EmitTableIndexExpr(std::vector<uint8_t>& code, const TableDef& td,
                          uint32_t bound, FuncState& fs, int depth,
                          const WasmModule& m, bool use_param0) {
    if (use_param0 && !m.functions[fs.my_def_idx].params.empty() &&
        m.functions[fs.my_def_idx].params[0].kind == WasmType::kI32) {
      code.push_back(0x20);  // `local.get 0`.
      EncodeU32(code, 0);
      if (td.is_table64) {
        code.push_back(0xAD);  // `i64.extend_i32_u`.
      }
    } else if (td.is_table64) {
      BuildExpr(code, WasmType::kI64, fs, depth - 1, m);
    } else {
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
    }
    const uint32_t safe_bound = std::max<uint32_t>(1, bound);
    if (td.is_table64) {
      code.push_back(0x42);
      EncodeI64(code, static_cast<int64_t>(safe_bound));
      code.push_back(0x82);  // `i64.rem_u`.
    } else {
      code.push_back(0x41);
      EncodeI32(code, static_cast<int32_t>(safe_bound));
      code.push_back(0x70);  // `i32.rem_u`.
    }
  }

  void EmitCallIndirect(std::vector<uint8_t>& code, FuncState& fs, int depth,
                        const WasmModule& m, bool use_param0) {
    for (const WasmValType& p : callee_sig_.params) {
      BuildExpr(code, p, fs, depth - 1, m);
    }
    EmitTableIndexExpr(code, tables_[0], static_cast<uint32_t>(table_callees_),
                       fs, depth, m, use_param0);
    code.push_back(0x11);  // `call_indirect`.
    EncodeU32(code, callee_type_idx_);
    EncodeU32(code, 0);
  }

  void EmitCallIndirectStmt(std::vector<uint8_t>& code, FuncState& fs,
                            int depth, const WasmModule& m, bool use_param0) {
    EmitCallIndirect(code, fs, depth, m, use_param0);
    if (!callee_sig_.results.empty()) {
      ConsumeResult(code, callee_sig_.results[0], fs);
    }
  }

  void EmitTableStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                     const WasmModule& m) {
    const uint32_t tab_idx = static_cast<uint32_t>(r_.Index(tables_.size()));
    const TableDef& td = tables_[tab_idx];
    const size_t k = r_.Index(5);
    if (k == 0) {
      // `table.set` with a matching reference value.
      EmitTableIndexExpr(code, td, td.min_size, fs, depth, m, false);
      if (td.elem_type.kind == WasmType::kFuncRef) {
        const size_t fn = total_imported_funcs_ +
                          r_.Index(std::max<size_t>(1, table_callees_));
        code.push_back(0xD2);  // `ref.func`.
        EncodeU32(code, static_cast<uint32_t>(fn));
      } else {
        BuildExpr(code, td.elem_type, fs, depth - 1, m);
      }
      code.push_back(0x26);  // `table.set`.
      EncodeU32(code, tab_idx);
    } else if (k == 1) {
      // `table.fill` over 1..2 slots (only on GC ref table so `tab` slots stay
      // populated with valid callees for `call_indirect`).
      if (tab_idx > 0) {
        EmitTableIndexExpr(code, td, 2, fs, depth, m, false);
        BuildExpr(code, td.elem_type, fs, depth - 1, m);
        if (td.is_table64) {
          code.push_back(0x42);
          EncodeI64(code, 2);
        } else {
          code.push_back(0x41);
          EncodeI32(code, 2);
        }
        EmitNumericOp(code, 17);  // `table.fill`.
        EncodeU32(code, tab_idx);
      } else {
        // `table.size`.
        EmitNumericOp(code, 16);
        EncodeU32(code, tab_idx);
        ConsumeResult(code, td.is_table64 ? WasmType::kI64 : WasmType::kI32,
                      fs);
      }
    } else if (k == 2) {
      // `table.copy` within the same table.
      EmitTableIndexExpr(code, td, 1, fs, depth, m, false);
      EmitTableIndexExpr(code, td, 1, fs, depth, m, false);
      if (td.is_table64) {
        code.push_back(0x42);
        EncodeI64(code, 1);
      } else {
        code.push_back(0x41);
        EncodeI32(code, 1);
      }
      EmitNumericOp(code, 14);  // `table.copy`.
      EncodeU32(code, tab_idx);
      EncodeU32(code, tab_idx);
    } else if (k == 3) {
      // `table.grow` by 0 or 1 element.
      if (td.elem_type.kind == WasmType::kFuncRef) {
        code.push_back(0xD2);
        EncodeU32(code, static_cast<uint32_t>(total_imported_funcs_));
      } else {
        BuildExpr(code, td.elem_type, fs, depth - 1, m);
      }
      if (td.is_table64) {
        code.push_back(0x42);
        EncodeI64(code, static_cast<int64_t>(r_.Index(2)));
      } else {
        code.push_back(0x41);
        EncodeI32(code, static_cast<int32_t>(r_.Index(2)));
      }
      EmitNumericOp(code, 15);  // `table.grow`.
      EncodeU32(code, tab_idx);
      ConsumeResult(code, td.is_table64 ? WasmType::kI64 : WasmType::kI32, fs);
    } else if (tab_idx == 0 && passive_elem_seg_ >= 0) {
      // `table.init` from passive segment 1 into table 0.
      if (td.is_table64) {
        code.push_back(0x42);
        EncodeI64(code, 0);
      } else {
        code.push_back(0x41);
        EncodeI32(code, 0);
      }
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, 1);
      EmitNumericOp(code, 12);  // `table.init`.
      EncodeU32(code, static_cast<uint32_t>(passive_elem_seg_));
      EncodeU32(code, 0);
    } else {
      EmitNumericOp(code, 16);  // `table.size`.
      EncodeU32(code, tab_idx);
      ConsumeResult(code, td.is_table64 ? WasmType::kI64 : WasmType::kI32, fs);
    }
  }

  void EmitGcStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                  const WasmModule& m) {
    const size_t k = r_.Index(7);
    if (k == 0) {
      // `struct.set` on a mutable field of `$S_base`, `$S_sub`, or `$S_alt`.
      const StructDef* sd = &s_base_def_;
      const size_t pick = r_.Index(3);
      if (pick == 1) {
        sd = &s_sub_def_;
      } else if (pick == 2) {
        sd = &s_alt_def_;
      }
      std::vector<uint32_t> mut_fields;
      for (size_t i = 0; i < sd->fields.size(); ++i) {
        if (sd->fields[i].mut) {
          mut_fields.push_back(static_cast<uint32_t>(i));
        }
      }
      if (!mut_fields.empty()) {
        const uint32_t fidx = r_.Pick(mut_fields);
        EmitNonNullStructExpr(code, *sd, fs, depth - 1, m);
        BuildExpr(code, sd->fields[fidx].type.Unpacked(), fs, depth - 1, m);
        EmitGcOp(code, 0x05);  // `struct.set`.
        EncodeU32(code, sd->type_idx);
        EncodeU32(code, fidx);
        return;
      }
    } else if (k == 1) {
      // `array.set` on `$A_num` or mutable `$A_ref`.
      const ArrayDef& ad =
          (a_ref_def_.mut && r_.Coin()) ? a_ref_def_ : a_num_def_;
      EmitNonNullArrayExpr(code, ad, fs, depth - 1, m);
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, 3);
      code.push_back(0x71);  // `i32.and 3` keeps index < 4.
      BuildExpr(code, ad.elem.Unpacked(), fs, depth - 1, m);
      EmitGcOp(code, 0x0E);  // `array.set`.
      EncodeU32(code, ad.type_idx);
      return;
    } else if (k == 2) {
      // `array.fill` on `$A_num`.
      EmitNonNullArrayExpr(code, a_num_def_, fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, static_cast<int32_t>(r_.Index(2)));
      BuildExpr(code, a_num_def_.elem.Unpacked(), fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, static_cast<int32_t>(1 + r_.Index(2)));
      EmitGcOp(code, 0x10);  // `array.fill`.
      EncodeU32(code, a_num_idx_);
      return;
    } else if (k == 3) {
      // `array.copy` between two `$A_num` arrays.
      EmitNonNullArrayExpr(code, a_num_def_, fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, 0);
      EmitNonNullArrayExpr(code, a_num_def_, fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, 2);
      EmitGcOp(code, 0x11);  // `array.copy`.
      EncodeU32(code, a_num_idx_);
      EncodeU32(code, a_num_idx_);
      return;
    } else if (k == 4 && passive_data_seg_0_ >= 0 &&
               a_num_def_.elem.IsNumericOrPacked()) {
      // `array.init_data` into `$A_num` from passive segment 0 (32 bytes).
      EmitNonNullArrayExpr(code, a_num_def_, fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, a_num_def_.elem.val_type.kind == WasmType::kV128 ? 1 : 2);
      EmitGcOp(code, 0x12);  // `array.init_data`.
      EncodeU32(code, a_num_idx_);
      EncodeU32(code, static_cast<uint32_t>(passive_data_seg_0_));
      return;
    } else if (k == 5) {
      // Self-contained `br_on_null` block.
      code.push_back(0x02);  // `block` (void).
      code.push_back(0x40);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      BuildExpr(code, WasmValType::RefNull(s_base_idx_), fs, depth - 1, m);
      code.push_back(0xD5);  // `br_on_null 0`.
      EncodeU32(code, 0);
      // Stack holds `(ref $S_base)`. Read field 0 and drop or store it.
      EmitGcOp(code, 0x02);  // `struct.get $S_base 0`.
      EncodeU32(code, s_base_idx_);
      EncodeU32(code, 0);
      ConsumeResult(code, WasmType::kI32, fs);
      code.push_back(0x0B);  // `end`.
      fs.block_stack.pop_back();
      return;
    } else {
      // Self-contained `br_on_cast` statement.
      code.push_back(0x02);  // `block` returning `(ref null $S_sub)`.
      EncodeS33(code, blk_sig_s_sub_null_);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      BuildExpr(code, WasmValType::RefNull(s_base_idx_), fs, depth - 1, m);
      EmitGcOp(code, 0x18);  // `br_on_cast`.
      code.push_back(0x03);  // `src_is_null = 1, res_is_null = 1`.
      EncodeU32(code, 0);    // Branch depth 0.
      EncodeS33(code, s_base_idx_);
      EncodeS33(code, s_sub_idx_);
      code.push_back(0x1A);  // `drop` uncaught `$S_base`.
      code.push_back(0xD0);  // `ref.null $S_sub`.
      EncodeS33(code, s_sub_idx_);
      code.push_back(0x0B);  // `end`.
      fs.block_stack.pop_back();
      ConsumeResult(code, WasmValType::RefNull(s_sub_idx_), fs);
      return;
    }
    code.push_back(0x01);  // `nop`.
  }

  void EmitEhStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                  const WasmModule& m) {
    // Keep the `start` function on caught patterns `0..1` so instantiation
    // never throws an uncaught exception.
    const size_t pattern = fs.is_start ? r_.Index(2) : r_.Index(4);
    if (pattern == 0 && i32_tag_idx_ >= 0) {
      // `block (result i32) try_table (result i32) (catch $i32_tag 0) ... end`.
      code.push_back(0x02);
      code.push_back(static_cast<uint8_t>(WasmType::kI32));
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      code.push_back(0x1F);  // `try_table`.
      code.push_back(static_cast<uint8_t>(WasmType::kI32));
      EncodeU32(code, 1);    // 1 catch clause.
      code.push_back(0x00);  // `catch`.
      EncodeU32(code, static_cast<uint32_t>(i32_tag_idx_));
      EncodeU32(code, 0);  // Outer `block` is depth 0 outside `try_table`.
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});

      // Conditionally throw `$i32_tag`.
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x04);  // `if` (void).
      code.push_back(0x40);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x08);  // `throw`.
      EncodeU32(code, static_cast<uint32_t>(i32_tag_idx_));
      code.push_back(0x0B);  // `end` of `if`.
      fs.block_stack.pop_back();

      if (fs.budget > 2) {
        BuildStmt(code, fs, depth - 1, m);
      }
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x0B);  // `end` of `try_table`.
      fs.block_stack.pop_back();
      code.push_back(0x0B);  // `end` of `block`.
      fs.block_stack.pop_back();
      ConsumeResult(code, WasmType::kI32, fs);
    } else if (pattern == 1 && void_tag_idx_ >= 0) {
      // `catch_all_ref` into `exnref` plus guarded `throw_ref` inside an outer
      // `catch_all` so unwinding runs without causing uncaught top-level fails.
      code.push_back(0x02);  // Outer void block catching rethrow.
      code.push_back(0x40);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      code.push_back(0x1F);  // Outer `try_table` (void).
      code.push_back(0x40);
      EncodeU32(code, 1);
      code.push_back(0x02);  // `catch_all`.
      EncodeU32(code, 0);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});

      code.push_back(0x02);  // Guard `block` (void) around `br_on_null`.
      code.push_back(0x40);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});

      code.push_back(0x02);  // Inner `block (result exnref)`.
      code.push_back(static_cast<uint8_t>(WasmType::kExnRef));
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      code.push_back(0x1F);  // Inner `try_table` (void).
      code.push_back(0x40);
      EncodeU32(code, 1);
      code.push_back(r_.Coin() ? 0x03 : 0x01);  // `catch_all_ref`/`catch_ref`.
      if (code.back() == 0x01) {
        EncodeU32(code, static_cast<uint32_t>(void_tag_idx_));
      }
      EncodeU32(code, 0);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});

      // Conditionally throw `$void_tag`.
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x04);
      code.push_back(0x40);
      code.push_back(0x08);  // `throw $void_tag`.
      EncodeU32(code, static_cast<uint32_t>(void_tag_idx_));
      code.push_back(0x0B);

      code.push_back(0x0B);  // `end` of inner `try_table`.
      fs.block_stack.pop_back();
      code.push_back(0xD0);  // `ref.null exn`.
      code.push_back(static_cast<uint8_t>(WasmType::kExnRef));
      code.push_back(0x0B);  // `end` of inner `block` leaves `exnref` on stack.
      fs.block_stack.pop_back();

      // Guard `throw_ref` with `br_on_null` so null `exnref` never traps.
      code.push_back(0xD5);  // `br_on_null 0`.
      EncodeU32(code, 0);
      if (r_.Coin()) {
        code.push_back(0x0A);  // `throw_ref`.
      } else {
        code.push_back(0x1A);  // `drop`.
      }
      code.push_back(0x0B);  // `end` of `br_on_null` guard.
      fs.block_stack.pop_back();

      code.push_back(0x0B);  // `end` of outer `try_table`.
      fs.block_stack.pop_back();
      code.push_back(0x0B);  // `end` of outer `block`.
      fs.block_stack.pop_back();
    } else if (pattern == 2 && jstag_idx_ >= 0 && !m.imports.empty() &&
               fs.allow_calls) {
      // Catch JS exceptions thrown by an imported JS function via `JSTag`.
      code.push_back(0x02);  // `block (result externref)`.
      code.push_back(static_cast<uint8_t>(WasmType::kExternRef));
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      code.push_back(0x1F);  // `try_table` (void).
      code.push_back(0x40);
      EncodeU32(code, 1);
      code.push_back(0x00);  // `catch $jstag 0`.
      EncodeU32(code, static_cast<uint32_t>(jstag_idx_));
      EncodeU32(code, 0);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});

      const size_t callee = r_.Index(m.imports.size());
      if (static_cast<int>(callee) == m.jspi_import &&
          static_cast<int>(fs.my_def_idx) != m.jspi_export) {
        // Avoid triggering `SuspendError` outside the promising export.
        code.push_back(0x01);
      } else {
        const WasmSignature& isig = m.imports[callee];
        for (const WasmValType& p : isig.params) {
          BuildExpr(code, p, fs, depth - 1, m);
        }
        code.push_back(0x10);
        EncodeU32(code, static_cast<uint32_t>(callee));
        if (!isig.results.empty()) {
          ConsumeResult(code, isig.results[0], fs);
        }
      }

      code.push_back(0x0B);  // `end` of `try_table`.
      fs.block_stack.pop_back();
      code.push_back(0xD0);  // `ref.null extern`.
      code.push_back(static_cast<uint8_t>(WasmType::kExternRef));
      code.push_back(0x0B);  // `end` of `block`.
      fs.block_stack.pop_back();
      ConsumeResult(code, WasmType::kExternRef, fs);
    } else if (i32_tag_idx_ >= 0) {
      // Occasional uncaught conditional `throw` so JS catches
      // `WebAssembly.Exception`.
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, 7);
      code.push_back(0x71);  // `i32.and 7`.
      code.push_back(0x45);  // `i32.eqz` (rare 1/8 chance).
      code.push_back(0x04);
      code.push_back(0x40);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x08);  // `throw $i32_tag`.
      EncodeU32(code, static_cast<uint32_t>(i32_tag_idx_));
      code.push_back(0x0B);
      fs.block_stack.pop_back();
    } else {
      code.push_back(0x01);
    }
  }

  void EmitBrTableStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                       const WasmModule& m) {
    // Emit a self-contained 2-level nested `block` with `br_table`.
    code.push_back(0x02);  // Outer void block (depth 1).
    code.push_back(0x40);
    fs.block_stack.push_back({false, false, WasmType::kI32});
    code.push_back(0x02);  // Inner void block (depth 0).
    code.push_back(0x40);
    fs.block_stack.push_back({false, false, WasmType::kI32});
    BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
    code.push_back(0x0E);  // `br_table`.
    EncodeU32(code, 2);    // 2 table targets + 1 default.
    EncodeU32(code, 0);
    EncodeU32(code, 1);
    EncodeU32(code, 0);
    code.push_back(0x0B);  // End inner block.
    fs.block_stack.pop_back();
    if (fs.budget > 2) {
      BuildStmt(code, fs, depth - 1, m);
    }
    code.push_back(0x0B);  // End outer block.
    fs.block_stack.pop_back();
  }

  bool TryEmitTailCallStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                           const WasmModule& m) {
    const WasmSignature& my_sig = m.functions[fs.my_def_idx];
    std::vector<size_t> cands;
    for (size_t i = 0; i < fs.my_func; ++i) {
      if (i >= m.imports.size() && i < total_imported_funcs_) {
        continue;
      }
      if (static_cast<int>(i) == m.jspi_import &&
          static_cast<int>(fs.my_def_idx) != m.jspi_export) {
        continue;
      }
      if (m.jspi_export >= 0 &&
          i == total_imported_funcs_ + static_cast<size_t>(m.jspi_export)) {
        continue;
      }
      if (all_func_sigs_[i].results == my_sig.results) {
        cands.push_back(i);
      }
    }
    if (cands.empty()) {
      return false;
    }
    // Emit inside a conditional `if` so code after `end` remains reachable.
    BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
    code.push_back(0x41);
    EncodeI32(code, 3);
    code.push_back(0x71);
    code.push_back(0x45);  // `i32.eqz`.
    code.push_back(0x04);  // `if` (void).
    code.push_back(0x40);
    fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});

    const bool can_indirect = m.table && table_callees_ > 0 &&
                              fs.my_def_idx >= table_callees_ &&
                              callee_sig_.results == my_sig.results;
    if (can_indirect && r_.Coin()) {
      for (const WasmValType& p : callee_sig_.params) {
        BuildExpr(code, p, fs, depth - 1, m);
      }
      EmitTableIndexExpr(code, tables_[0],
                         static_cast<uint32_t>(table_callees_), fs, depth, m,
                         false);
      code.push_back(0x13);  // `return_call_indirect`.
      EncodeU32(code, callee_type_idx_);
      EncodeU32(code, 0);
    } else {
      const size_t callee = r_.Pick(cands);
      for (const WasmValType& p : all_func_sigs_[callee].params) {
        BuildExpr(code, p, fs, depth - 1, m);
      }
      if (callee >= total_imported_funcs_ && r_.Coin()) {
        code.push_back(0xD2);  // `ref.func` defined function.
        EncodeU32(code, static_cast<uint32_t>(callee));
        code.push_back(0x15);  // `return_call_ref`.
        EncodeU32(code, all_func_type_idxs_[callee]);
      } else {
        code.push_back(0x12);  // `return_call`.
        EncodeU32(code, static_cast<uint32_t>(callee));
      }
    }
    code.push_back(0x0B);  // `end` of `if`.
    fs.block_stack.pop_back();
    return true;
  }

  void EmitJsStringStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                        const WasmModule& m) {
    // Exercise `wasm:js-string` builtins with imported string constants and
    // mutable `i16` arrays.
    if (js_string_imports_.size() < 12) {
      code.push_back(0x01);
      return;
    }
    const size_t op = r_.Index(6);
    if (op == 0) {
      // `concat(str0, str1)` -> `length(...)`.
      code.push_back(0x23);
      EncodeU32(code, str_const_global_0_);
      code.push_back(0x23);
      EncodeU32(code, str_const_global_1_);
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[6].func_idx);  // `concat`.
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[1].func_idx);  // `length`.
      ConsumeResult(code, WasmType::kI32, fs);
    } else if (op == 1) {
      // `fromCharCode(65 + (x & 31))` -> `charCodeAt(s, 0)`.
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, 31);
      code.push_back(0x71);
      code.push_back(0x41);
      EncodeI32(code, 65);
      code.push_back(0x6A);
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[2].func_idx);  // `fromCharCode`.
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[4].func_idx);  // `charCodeAt`.
      ConsumeResult(code, WasmType::kI32, fs);
    } else if (op == 2) {
      // `substring(str1, 0, 3)` + `equals`/`compare`.
      code.push_back(0x23);
      EncodeU32(code, str_const_global_1_);
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, 3);
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[7].func_idx);  // `substring`.
      code.push_back(0x23);
      EncodeU32(code, str_const_global_0_);
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[r_.Coin() ? 8 : 9].func_idx);
      ConsumeResult(code, WasmType::kI32, fs);
    } else if (op == 3) {
      // `fromCharCodeArray` from a 4-element `i16` array.
      code.push_back(0x41);
      EncodeI32(code, 65);
      code.push_back(0x41);
      EncodeI32(code, 4);
      EmitGcOp(code, 0x06);  // `array.new str_array_i16_idx_`.
      EncodeU32(code, str_array_i16_idx_);
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, 4);
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[10].func_idx);  // `fromCharCodeArray`.
      ConsumeResult(code, WasmType::kExternRef, fs);
    } else if (op == 4) {
      // `intoCharCodeArray(str0, new_array(8), 0)`.
      code.push_back(0x23);
      EncodeU32(code, str_const_global_0_);
      code.push_back(0x41);
      EncodeI32(code, 8);
      EmitGcOp(code, 0x07);  // `array.new_default str_array_i16_idx_`.
      EncodeU32(code, str_array_i16_idx_);
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[11].func_idx);  // `intoCharCodeArray`.
      ConsumeResult(code, WasmType::kI32, fs);
    } else {
      // `test(externref)`.
      BuildExpr(code, WasmType::kExternRef, fs, depth - 1, m);
      code.push_back(0x10);
      EncodeU32(code, js_string_imports_[0].func_idx);  // `test`.
      ConsumeResult(code, WasmType::kI32, fs);
    }
  }

  void BuildStmt(std::vector<uint8_t>& code, FuncState& fs, int depth,
                 const WasmModule& m) {
    if (fs.budget <= 0) {
      return;
    }
    --fs.budget;

    const size_t choice = r_.Index(15);
    switch (choice) {
      case 0: {  // `local.set`.
        if (!fs.user_locals.empty()) {
          const size_t idx = r_.Index(fs.user_locals.size());
          BuildExpr(code, fs.user_locals[idx], fs, depth - 1, m);
          code.push_back(0x21);
          EncodeU32(code, static_cast<uint32_t>(idx));
          return;
        }
        break;
      }
      case 1: {  // `global.set` on a mutable global.
        std::vector<size_t> cands;
        for (size_t i = 0; i < globals_.size(); ++i) {
          if (globals_[i].mut) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          const size_t idx = r_.Pick(cands);
          BuildExpr(code, globals_[idx].type, fs, depth - 1, m);
          code.push_back(0x24);
          EncodeU32(code, static_cast<uint32_t>(idx));
          return;
        }
        break;
      }
      case 2: {  // Masked memory store (scalar, SIMD, or atomic).
        if (m.memory) {
          EmitStoreStmt(code, fs, depth, m);
          return;
        }
        break;
      }
      case 3: {  // Bounded `loop` with a dedicated countdown local.
        if (fs.loop_depth < kMaxLoopNesting && depth > 1 && fs.budget >= 2) {
          const uint32_t ctr = static_cast<uint32_t>(
              fs.loop_counter_base + static_cast<size_t>(fs.loop_depth));
          const bool prev_allow_calls = fs.allow_calls;
          const bool allow_loop_calls =
              prev_allow_calls && fs.loop_depth == 0 && r_.Chance(0.25);
          const int max_iters =
              allow_loop_calls
                  ? std::min(4, kWasmMaxLoopCount.AsInt())
                  : (fs.loop_depth > 0 ? std::min(4, kWasmMaxLoopCount.AsInt())
                                       : kWasmMaxLoopCount.AsInt());
          const int32_t count =
              1 +
              static_cast<int32_t>(r_.Index(static_cast<size_t>(max_iters)));
          code.push_back(0x41);  // `i32.const count`.
          EncodeI32(code, count);
          code.push_back(0x21);  // `local.set ctr`.
          EncodeU32(code, ctr);
          code.push_back(0x03);  // `loop`.
          code.push_back(0x40);  // Void block type.
          fs.block_stack.push_back(
              {/*is_loop=*/true, /*has_result=*/false, WasmType::kI32});
          ++fs.loop_depth;
          fs.allow_calls = allow_loop_calls;

          const int body_stmts = 1 + static_cast<int>(r_.Index(3));
          for (int s = 0; s < body_stmts && fs.budget > 1; ++s) {
            BuildStmt(code, fs, depth - 1, m);
          }

          fs.allow_calls = prev_allow_calls;
          --fs.loop_depth;
          fs.block_stack.pop_back();

          code.push_back(0x20);  // `local.get ctr`.
          EncodeU32(code, ctr);
          code.push_back(0x41);  // `i32.const 1`.
          EncodeI32(code, 1);
          code.push_back(0x6B);  // `i32.sub`.
          code.push_back(0x22);  // `local.tee ctr`.
          EncodeU32(code, ctr);
          code.push_back(0x0D);  // `br_if 0`.
          EncodeU32(code, 0);
          code.push_back(0x0B);  // `end`.
          return;
        }
        break;
      }
      case 4: {  // Void `if`/`else`.
        if (depth > 1 && fs.budget >= 2) {
          BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
          code.push_back(0x04);  // `if`.
          code.push_back(0x40);
          fs.block_stack.push_back({false, false, WasmType::kI32});
          const int then_n = 1 + static_cast<int>(r_.Index(2));
          for (int s = 0; s < then_n && fs.budget > 1; ++s) {
            BuildStmt(code, fs, depth - 1, m);
          }
          code.push_back(0x05);  // `else`.
          const int else_n = 1 + static_cast<int>(r_.Index(2));
          for (int s = 0; s < else_n && fs.budget > 1; ++s) {
            BuildStmt(code, fs, depth - 1, m);
          }
          code.push_back(0x0B);  // `end`.
          fs.block_stack.pop_back();
          return;
        }
        break;
      }
      case 5: {  // Void `block` or `br_table`.
        if (depth > 1 && fs.budget >= 2) {
          if (r_.Coin()) {
            EmitBrTableStmt(code, fs, depth, m);
            return;
          }
          code.push_back(0x02);  // `block`.
          code.push_back(0x40);
          fs.block_stack.push_back({false, false, WasmType::kI32});
          const int stmts = 1 + static_cast<int>(r_.Index(3));
          for (int s = 0; s < stmts && fs.budget > 1; ++s) {
            BuildStmt(code, fs, depth - 1, m);
          }
          code.push_back(0x0B);  // `end`.
          fs.block_stack.pop_back();
          return;
        }
        break;
      }
      case 6: {  // Forward `br_if` to an enclosing non-loop label.
        std::vector<size_t> cands;
        for (size_t i = 0; i < fs.block_stack.size(); ++i) {
          if (!fs.block_stack[i].is_loop) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          const size_t target_idx = r_.Pick(cands);
          const uint32_t rel_depth =
              static_cast<uint32_t>(fs.block_stack.size() - 1 - target_idx);
          const BlockLabel target = fs.block_stack[target_idx];
          if (target.has_result) {
            BuildExpr(code, target.result, fs, depth - 1, m);
          }
          BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
          code.push_back(0x0D);  // `br_if`.
          EncodeU32(code, rel_depth);
          if (target.has_result) {
            code.push_back(0x1A);  // `drop` if branch is not taken.
          }
          return;
        }
        break;
      }
      case 7: {  // Direct `call`, typed `call_ref`, or conditional tail call.
        if (fs.allow_calls && fs.my_func > 0) {
          if (r_.Chance(kWasmTailCallProb()) &&
              TryEmitTailCallStmt(code, fs, depth, m)) {
            return;
          }
          const size_t callee = r_.Index(fs.my_func);
          // Keep JSPI suspending import and export outside non-JSPI callers.
          if (static_cast<int>(callee) == m.jspi_import &&
              static_cast<int>(fs.my_def_idx) != m.jspi_export) {
            break;
          }
          if (m.jspi_export >= 0 &&
              callee ==
                  total_imported_funcs_ + static_cast<size_t>(m.jspi_export)) {
            break;
          }
          // Skip raw calls to `wasm:js-string` here. `EmitJsStringStmt` drives
          // them with safe string arguments.
          if (callee >= m.imports.size() && callee < total_imported_funcs_) {
            if (has_js_string_) {
              EmitJsStringStmt(code, fs, depth, m);
              return;
            }
            break;
          }
          const WasmSignature& sig = all_func_sigs_[callee];
          for (const WasmValType& p : sig.params) {
            BuildExpr(code, p, fs, depth - 1, m);
          }
          if (callee >= total_imported_funcs_ && r_.Coin()) {
            code.push_back(0xD2);  // `ref.func` defined function.
            EncodeU32(code, static_cast<uint32_t>(callee));
            code.push_back(0x14);  // `call_ref`.
            EncodeU32(code, all_func_type_idxs_[callee]);
          } else {
            code.push_back(0x10);  // `call`.
            EncodeU32(code, static_cast<uint32_t>(callee));
          }
          if (!sig.results.empty()) {
            ConsumeResult(code, sig.results[0], fs);
          }
          return;
        }
        break;
      }
      case 8: {  // `call_indirect` into the function table.
        if (fs.allow_calls && m.table && table_callees_ > 0 &&
            fs.my_def_idx >= table_callees_) {
          EmitCallIndirectStmt(code, fs, depth, m, /*use_param0=*/r_.Coin());
          return;
        }
        break;
      }
      case 9: {  // Bulk memory ops and `atomic.fence`.
        if (m.memory && r_.Chance(kWasmBulkMemoryProb())) {
          EmitBulkMemoryStmt(code, fs, depth, m);
          return;
        }
        break;
      }
      case 10: {  // WasmGC struct/array mutation and cast-branch statements.
        if (has_gc_ && depth > 1) {
          EmitGcStmt(code, fs, depth, m);
          return;
        }
        break;
      }
      case 11: {  // Exception handling (`try_table`, `throw`, `throw_ref`).
        if (has_eh_ && depth > 1) {
          EmitEhStmt(code, fs, depth, m);
          return;
        }
        break;
      }
      case 12: {  // Table manipulation (`table.set`/`fill`/`copy`/`grow`).
        if (!tables_.empty() && depth > 1) {
          EmitTableStmt(code, fs, depth, m);
          return;
        }
        break;
      }
      case 13: {  // JS string builtins (`wasm:js-string`).
        if (has_js_string_ && fs.allow_calls) {
          EmitJsStringStmt(code, fs, depth, m);
          return;
        }
        break;
      }
      case 14: {  // Evaluate a `v128` expression and consume into a local.
        if (has_simd_) {
          BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
          ConsumeResult(code, WasmType::kV128, fs);
          return;
        }
        break;
      }
    }

    // Fallback: mutate a local or emit `nop` to keep the stack neutral.
    if (!fs.user_locals.empty()) {
      const size_t idx = r_.Index(fs.user_locals.size());
      BuildExpr(code, fs.user_locals[idx], fs, depth - 1, m);
      code.push_back(0x21);
      EncodeU32(code, static_cast<uint32_t>(idx));
    } else {
      code.push_back(0x01);  // `nop`.
    }
  }

  void EmitNonNullStructExpr(std::vector<uint8_t>& code, const StructDef& sd,
                             FuncState& fs, int depth, const WasmModule& m) {
    if (sd.defaultable && (depth <= 0 || fs.budget <= 0 || r_.Coin())) {
      EmitGcOp(code, 0x01);  // `struct.new_default`.
      EncodeU32(code, sd.type_idx);
      return;
    }
    for (const StructField& f : sd.fields) {
      if (depth > 0 && fs.budget > 0) {
        BuildExpr(code, f.type.Unpacked(), fs, depth - 1, m);
      } else {
        EmitConst(code, f.type.Unpacked());
      }
    }
    EmitGcOp(code, 0x00);  // `struct.new`.
    EncodeU32(code, sd.type_idx);
  }

  void EmitNonNullArrayExpr(std::vector<uint8_t>& code, const ArrayDef& ad,
                            FuncState& fs, int depth, const WasmModule& m) {
    // Keep length in `4..19` so masked element accesses `& 3` stay in bounds.
    if (depth <= 0 || fs.budget <= 0) {
      code.push_back(0x41);
      EncodeI32(code, 4);
      EmitGcOp(code, 0x07);  // `array.new_default`.
      EncodeU32(code, ad.type_idx);
      return;
    }
    const size_t k = r_.Index(5);
    if (k == 0 && ad.defaultable) {
      code.push_back(0x41);
      EncodeI32(code, static_cast<int32_t>(4 + r_.Index(8)));
      EmitGcOp(code, 0x07);  // `array.new_default`.
      EncodeU32(code, ad.type_idx);
    } else if (k == 1) {
      BuildExpr(code, ad.elem.Unpacked(), fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, static_cast<int32_t>(4 + r_.Index(8)));
      EmitGcOp(code, 0x06);  // `array.new`.
      EncodeU32(code, ad.type_idx);
    } else if (k == 2) {
      for (int i = 0; i < 4; ++i) {
        BuildExpr(code, ad.elem.Unpacked(), fs, depth - 1, m);
      }
      EmitGcOp(code, 0x08);  // `array.new_fixed`.
      EncodeU32(code, ad.type_idx);
      EncodeU32(code, 4);
    } else if (k == 3 && ad.elem.IsNumericOrPacked() &&
               passive_data_seg_0_ >= 0 &&
               ad.elem.val_type.kind != WasmType::kV128) {
      // Passive segment 0 has 32 bytes: offset 0, len 4 fits up to 8-byte elem.
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, 4);
      EmitGcOp(code, 0x09);  // `array.new_data`.
      EncodeU32(code, ad.type_idx);
      EncodeU32(code, static_cast<uint32_t>(passive_data_seg_0_));
    } else if (k == 4 && !ad.elem.IsNumericOrPacked() &&
               ad.elem.val_type.kind == WasmType::kFuncRef &&
               passive_elem_seg_ >= 0 && m.table_size >= 1) {
      // Passive segment 1 always holds at least 4 entries so the array keeps
      // length >= 4 for masked element accesses.
      code.push_back(0x41);
      EncodeI32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, 4);
      EmitGcOp(code, 0x0A);  // `array.new_elem`.
      EncodeU32(code, ad.type_idx);
      EncodeU32(code, static_cast<uint32_t>(passive_elem_seg_));
    } else {
      code.push_back(0x41);
      EncodeI32(code, 4);
      EmitGcOp(code, 0x07);  // `array.new_default`.
      EncodeU32(code, ad.type_idx);
    }
  }

  void EmitSimdExpr(std::vector<uint8_t>& code, FuncState& fs, int depth,
                    const WasmModule& m) {
    if (depth <= 0 || fs.budget <= 0) {
      EmitConst(code, WasmType::kV128);
      return;
    }
    --fs.budget;

    const size_t choice = r_.Index(11);
    switch (choice) {
      case 0: {  // `v128.const`.
        EmitConst(code, WasmType::kV128);
        return;
      }
      case 1: {  // `local.get` / `global.get` of `v128`.
        std::vector<size_t> cands;
        for (size_t i = 0; i < fs.user_locals.size(); ++i) {
          if (fs.user_locals[i].kind == WasmType::kV128) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          code.push_back(0x20);
          EncodeU32(code, static_cast<uint32_t>(r_.Pick(cands)));
          return;
        }
        EmitConst(code, WasmType::kV128);
        return;
      }
      case 2: {  // Splat from scalar (`i8x16`..`f64x2`).
        static constexpr uint32_t kOps[6] = {0x0F, 0x10, 0x11,
                                             0x12, 0x13, 0x14};
        static constexpr WasmType kScalar[6] = {WasmType::kI32, WasmType::kI32,
                                                WasmType::kI32, WasmType::kI64,
                                                WasmType::kF32, WasmType::kF64};
        const size_t k = r_.Index(6);
        BuildExpr(code, kScalar[k], fs, depth - 1, m);
        EmitSimdOp(code, kOps[k]);
        return;
      }
      case 3: {  // `replace_lane`.
        static constexpr uint32_t kOps[6] = {0x17, 0x1A, 0x1C,
                                             0x1E, 0x20, 0x22};
        static constexpr WasmType kScalar[6] = {WasmType::kI32, WasmType::kI32,
                                                WasmType::kI32, WasmType::kI64,
                                                WasmType::kF32, WasmType::kF64};
        static constexpr uint8_t kLanes[6] = {16, 8, 4, 2, 4, 2};
        const size_t k = r_.Index(6);
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        BuildExpr(code, kScalar[k], fs, depth - 1, m);
        EmitSimdOp(code, kOps[k]);
        code.push_back(static_cast<uint8_t>(r_.Index(kLanes[k])));
        return;
      }
      case 4: {  // Unary vector op (integer, float, bitwise, convert, relaxed).
        static constexpr uint32_t kOps[] = {
            0x4D,                // `v128.not`
            0x5E,  0x5F,         // `demote_f64x2_zero`, `promote_low_f32x4`
            0x60,  0x61,  0x62,  // `i8x16.abs/neg/popcnt`
            0x67,  0x68,  0x69,  0x6A,  // `f32x4.ceil/floor/trunc/nearest`
            0x74,  0x75,  0x7A,  0x94,  // `f64x2.ceil/floor/trunc/nearest`
            0x7C,  0x7D,  0x7E,  0x7F,  // `extadd_pairwise_*`
            0x80,  0x81,  0x87,  0x88,  0x89, 0x8A,  // `i16x8` abs/neg/extend
            0xA0,  0xA1,  0xA7,  0xA8,  0xA9, 0xAA,  // `i32x4` abs/neg/extend
            0xC0,  0xC1,  0xC7,  0xC8,  0xC9, 0xCA,  // `i64x2` abs/neg/extend
            0xE0,  0xE1,  0xE3,                      // `f32x4.abs/neg/sqrt`
            0xEC,  0xED,  0xEF,                      // `f64x2.abs/neg/sqrt`
            0xF8,  0xF9,  0xFA,  0xFB,  0xFC, 0xFD, 0xFE, 0xFF,  // converts
            0x101, 0x102, 0x103, 0x104,  // relaxed truncs
        };
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        EmitSimdOp(code, kOps[r_.Index(sizeof(kOps) / sizeof(kOps[0]))]);
        return;
      }
      case 5: {  // Binary vector op (int/float arithmetic, compares, relaxed).
        static constexpr uint32_t kOps[] = {
            0x0E,  // `i8x16.swizzle`
            0x23,  0x24,  0x25,  0x26,  0x2D, 0x2F, 0x37, 0x39,  0x41,  0x43,
            0x47,
            0x49,                       // compares
            0x4E,  0x4F,  0x50,  0x51,  // `v128.and/andnot/or/xor`
            0x65,  0x66,                // `i8x16.narrow`
            0x6E,  0x6F,  0x70,  0x71,  0x72, 0x73, 0x76, 0x77,  0x78,  0x79,
            0x7B,  0x82,  0x85,  0x86,  0x8E, 0x8F, 0x90, 0x91,  0x92,  0x93,
            0x95,  0x96,  0x98,  0x9B,  0x9C, 0x9D, 0x9E, 0x9F,  0xAE,  0xB1,
            0xB5,  0xB6,  0xB7,  0xB8,  0xB9, 0xBA, 0xBC, 0xBD,  0xBE,  0xBF,
            0xCE,  0xD1,  0xD5,  0xD6,  0xD7, 0xD8, 0xD9, 0xDC,  0xDD,  0xDE,
            0xDF,  0xE4,  0xE5,  0xE6,  0xE7, 0xE8, 0xE9, 0xEA,  0xEB,  0xF0,
            0xF1,  0xF2,  0xF3,  0xF4,  0xF5, 0xF6, 0xF7, 0x100, 0x10D, 0x10E,
            0x10F, 0x110, 0x111, 0x112,  // relaxed
        };
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        EmitSimdOp(code, kOps[r_.Index(sizeof(kOps) / sizeof(kOps[0]))]);
        return;
      }
      case 6: {  // `i8x16.shuffle`.
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        EmitSimdOp(code, 0x0D);
        for (int i = 0; i < 16; ++i) {
          code.push_back(static_cast<uint8_t>(r_.Index(32)));
        }
        return;
      }
      case 7: {  // Vector shifts (`(v128, i32) -> v128`).
        static constexpr uint32_t kOps[] = {0x6B, 0x6C, 0x6D, 0x8B, 0x8C, 0x8D,
                                            0xAB, 0xAC, 0xAD, 0xCB, 0xCC, 0xCD};
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        EmitSimdOp(code, kOps[r_.Index(sizeof(kOps) / sizeof(kOps[0]))]);
        return;
      }
      case 8: {  // Ternary (`v128.bitselect`, relaxed `qfma`/`laneselect`).
        static constexpr uint32_t kOps[] = {0x52,  0x105, 0x106, 0x107, 0x108,
                                            0x109, 0x10A, 0x10B, 0x10C, 0x113};
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        EmitSimdOp(code, kOps[r_.Index(sizeof(kOps) / sizeof(kOps[0]))]);
        return;
      }
      case 9: {  // SIMD memory load (`v128.load*` and `v128.load*_lane`).
        if (m.memory) {
          const uint32_t mem_idx = PickMemoryIndex(m);
          EmitSafeAddress(code, mem_idx, fs, depth, m, /*force_aligned=*/false);
          if (r_.Coin()) {
            BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
            static constexpr uint32_t kOps[4] = {0x54, 0x55, 0x56, 0x57};
            static constexpr uint32_t kAligns[4] = {0, 1, 2, 3};
            static constexpr uint8_t kLanes[4] = {16, 8, 4, 2};
            const size_t k = r_.Index(4);
            EmitSimdOp(code, kOps[k]);
            EmitMemArg(code, mem_idx,
                       static_cast<uint32_t>(r_.Index(kAligns[k] + 1)),
                       PickMemOffset(/*align8=*/false));
            code.push_back(static_cast<uint8_t>(r_.Index(kLanes[k])));
          } else {
            static constexpr uint32_t kOps[] = {0x00, 0x01, 0x02, 0x03, 0x04,
                                                0x05, 0x06, 0x07, 0x08, 0x09,
                                                0x0A, 0x5C, 0x5D};
            static constexpr uint32_t kAligns[] = {4, 3, 3, 3, 3, 3, 3,
                                                   0, 1, 2, 3, 2, 3};
            const size_t k = r_.Index(sizeof(kOps) / sizeof(kOps[0]));
            EmitSimdOp(code, kOps[k]);
            EmitMemArg(code, mem_idx,
                       static_cast<uint32_t>(r_.Index(kAligns[k] + 1)),
                       PickMemOffset(/*align8=*/false));
          }
          return;
        }
        break;
      }
      case 10: {  // `select` on `v128`.
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        code.push_back(0x1B);
        return;
      }
    }
    EmitConst(code, WasmType::kV128);
  }

  void EmitSimdToScalarExpr(std::vector<uint8_t>& code, const WasmValType& res,
                            FuncState& fs, int depth, const WasmModule& m) {
    BuildExpr(code, WasmType::kV128, fs, depth - 1, m);
    if (res.kind == WasmType::kI32) {
      if (r_.Coin()) {
        static constexpr uint32_t kPreds[] = {0x53, 0x63, 0x64, 0x83, 0x84,
                                              0xA3, 0xA4, 0xC3, 0xC4};
        EmitSimdOp(code, kPreds[r_.Index(sizeof(kPreds) / sizeof(kPreds[0]))]);
      } else {
        static constexpr uint32_t kOps[5] = {0x15, 0x16, 0x18, 0x19, 0x1B};
        static constexpr uint8_t kLanes[5] = {16, 16, 8, 8, 4};
        const size_t k = r_.Index(5);
        EmitSimdOp(code, kOps[k]);
        code.push_back(static_cast<uint8_t>(r_.Index(kLanes[k])));
      }
    } else if (res.kind == WasmType::kI64) {
      EmitSimdOp(code, 0x1D);  // `i64x2.extract_lane`.
      code.push_back(static_cast<uint8_t>(r_.Index(2)));
    } else if (res.kind == WasmType::kF32) {
      EmitSimdOp(code, 0x1F);  // `f32x4.extract_lane`.
      code.push_back(static_cast<uint8_t>(r_.Index(4)));
    } else {
      EmitSimdOp(code, 0x21);  // `f64x2.extract_lane`.
      code.push_back(static_cast<uint8_t>(r_.Index(2)));
    }
  }

  void EmitGcRefExpr(std::vector<uint8_t>& code, const WasmValType& res,
                     FuncState& fs, int depth, const WasmModule& m) {
    if (depth <= 0 || fs.budget <= 0) {
      EmitConst(code, res);
      return;
    }
    --fs.budget;

    // Try reusing a matching local first.
    if (r_.Coin()) {
      std::vector<size_t> cands;
      for (size_t i = 0; i < fs.user_locals.size(); ++i) {
        if (fs.user_locals[i] == res) {
          cands.push_back(i);
        }
      }
      if (!cands.empty()) {
        code.push_back(0x20);
        EncodeU32(code, static_cast<uint32_t>(r_.Pick(cands)));
        return;
      }
    }

    if (res.kind == WasmType::kExternRef) {
      const size_t k = r_.Index(3);
      if (k == 0 && has_js_string_) {
        code.push_back(0x23);
        EncodeU32(code, r_.Coin() ? str_const_global_0_ : str_const_global_1_);
        return;
      }
      if (k == 1 && has_gc_) {
        BuildExpr(code, WasmType::kAnyRef, fs, depth - 1, m);
        EmitGcOp(code, 0x1B);  // `extern.convert_any`.
        return;
      }
      EmitConst(code, res);
      return;
    }

    if (res.kind == WasmType::kFuncRef) {
      if (r_.Coin()) {
        const size_t fn = total_imported_funcs_ + r_.Index(m.functions.size());
        code.push_back(0xD2);  // `ref.func`.
        EncodeU32(code, static_cast<uint32_t>(fn));
        return;
      }
      if (!tables_.empty() && tables_[0].elem_type.kind == WasmType::kFuncRef) {
        EmitTableIndexExpr(code, tables_[0], tables_[0].min_size, fs, depth, m,
                           false);
        code.push_back(0x25);  // `table.get 0`.
        EncodeU32(code, 0);
        return;
      }
      EmitConst(code, res);
      return;
    }

    if (res.kind == WasmType::kExnRef) {
      code.push_back(0xD0);  // `ref.null exn`.
      code.push_back(static_cast<uint8_t>(WasmType::kExnRef));
      return;
    }

    if (!has_gc_) {
      EmitConst(code, res);
      return;
    }

    // Concrete struct or array references.
    if (res.IsConcreteRef()) {
      if (res.type_idx == s_base_idx_) {
        const size_t k = r_.Index(5);
        if (k == 0) {
          EmitNonNullStructExpr(code, s_base_def_, fs, depth - 1, m);
          return;
        }
        if (k == 1) {
          // Upcast `$S_sub` to `(ref null $S_base)`.
          EmitNonNullStructExpr(code, s_sub_def_, fs, depth - 1, m);
          return;
        }
        if (k == 2) {
          EmitNonNullStructExpr(code, s_alt_def_, fs, depth - 1, m);
          return;
        }
        if (k == 3 && res.IsNullable()) {
          // `ref.cast null $S_base` on an upcasted `$S_sub` (succeeds!).
          EmitNonNullStructExpr(code, s_sub_def_, fs, depth - 1, m);
          EmitGcOp(code, 0x17);  // `ref.cast null`.
          EncodeS33(code, s_base_idx_);
          return;
        }
        if (k == 4 && res.IsNullable()) {
          // Typed `select (ref null $S_base)`.
          EmitNonNullStructExpr(code, s_sub_def_, fs, depth - 1, m);
          EmitNonNullStructExpr(code, s_alt_def_, fs, depth - 1, m);
          BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
          code.push_back(0x1C);
          EncodeU32(code, 1);
          EncodeValType(code, res);
          return;
        }
        EmitConst(code, res);
        return;
      }
      if (res.type_idx == s_sub_idx_) {
        if (r_.Coin()) {
          EmitNonNullStructExpr(code, s_sub_def_, fs, depth - 1, m);
        } else if (res.IsNullable()) {
          // Cast `$S_sub` through `anyref` back to `(ref null $S_sub)`.
          EmitNonNullStructExpr(code, s_sub_def_, fs, depth - 1, m);
          EmitGcOp(code, 0x17);
          EncodeS33(code, s_sub_idx_);
        } else {
          EmitNonNullStructExpr(code, s_sub_def_, fs, depth - 1, m);
        }
        return;
      }
      if (res.type_idx == s_alt_idx_) {
        EmitNonNullStructExpr(code, s_alt_def_, fs, depth - 1, m);
        return;
      }
      if (res.type_idx == a_num_idx_) {
        EmitNonNullArrayExpr(code, a_num_def_, fs, depth - 1, m);
        return;
      }
      if (res.type_idx == a_ref_idx_) {
        EmitNonNullArrayExpr(code, a_ref_def_, fs, depth - 1, m);
        return;
      }
      EmitConst(code, res);
      return;
    }

    // Abstract `i31ref`, `structref`, `arrayref`, `eqref`, `anyref`, and
    // non-null abstract `(ref ...)`.
    const uint8_t ht = res.kind == WasmType::kRef
                           ? res.heap_type
                           : static_cast<uint8_t>(res.kind);
    if (ht == static_cast<uint8_t>(WasmType::kI31Ref)) {
      if (r_.Coin() || !res.IsNullable()) {
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        EmitGcOp(code, 0x1C);  // `ref.i31`.
      } else {
        EmitConst(code, res);
      }
      return;
    }
    if (ht == static_cast<uint8_t>(WasmType::kStructRef)) {
      const size_t k = r_.Index(3);
      EmitNonNullStructExpr(
          code, k == 0 ? s_base_def_ : (k == 1 ? s_sub_def_ : s_alt_def_), fs,
          depth - 1, m);
      return;
    }
    if (ht == static_cast<uint8_t>(WasmType::kArrayRef)) {
      EmitNonNullArrayExpr(code, r_.Coin() ? a_num_def_ : a_ref_def_, fs,
                           depth - 1, m);
      return;
    }
    // `anyref` or `eqref`: pick across `i31`, structs, arrays,
    // `any.convert_extern`, `br_on_non_null`, and `br_on_cast_fail`.
    const size_t k = r_.Index(7);
    if (k == 0) {
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      EmitGcOp(code, 0x1C);  // `ref.i31`.
    } else if (k == 1) {
      EmitNonNullStructExpr(code, r_.Coin() ? s_base_def_ : s_sub_def_, fs,
                            depth - 1, m);
    } else if (k == 2) {
      EmitNonNullArrayExpr(code, r_.Coin() ? a_num_def_ : a_ref_def_, fs,
                           depth - 1, m);
    } else if (k == 3 && ht == static_cast<uint8_t>(WasmType::kAnyRef) &&
               res.IsNullable()) {
      BuildExpr(code, WasmType::kExternRef, fs, depth - 1, m);
      EmitGcOp(code, 0x1A);  // `any.convert_extern`.
    } else if (k == 4 && res.IsNullable()) {
      // Self-contained `br_on_non_null`.
      code.push_back(0x02);  // `block (result anyref/eqref)`.
      code.push_back(ht);
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      BuildExpr(code, res, fs, depth - 1, m);
      code.push_back(0xD6);  // `br_on_non_null 0`.
      EncodeU32(code, 0);
      code.push_back(0x41);
      EncodeI32(code, 7);
      EmitGcOp(code, 0x1C);  // `ref.i31`.
      code.push_back(0x0B);
      fs.block_stack.pop_back();
    } else if (k == 5 && ht == static_cast<uint8_t>(WasmType::kAnyRef) &&
               res.IsNullable()) {
      // Self-contained `br_on_cast_fail` from `anyref` to `(ref $S_sub)`.
      code.push_back(0x02);  // `block (result anyref)`.
      code.push_back(static_cast<uint8_t>(WasmType::kAnyRef));
      fs.block_stack.push_back({/*is_loop=*/true, false, WasmType::kI32});
      BuildExpr(code, WasmType::kAnyRef, fs, depth - 1, m);
      EmitGcOp(code, 0x19);  // `br_on_cast_fail`.
      code.push_back(0x01);  // `src_is_null = 1, res_is_null = 0`.
      EncodeU32(code, 0);
      code.push_back(static_cast<uint8_t>(WasmType::kAnyRef));
      EncodeS33(code, s_sub_idx_);
      code.push_back(0x0B);
      fs.block_stack.pop_back();
    } else if (k == 6 && !res.IsNullable()) {
      // `ref.as_non_null` on a non-null producer (never traps).
      EmitNonNullStructExpr(code, s_base_def_, fs, depth - 1, m);
      code.push_back(0xD4);  // `ref.as_non_null`.
    } else {
      EmitConst(code, res);
    }
  }

  bool TryEmitGcToScalarExpr(std::vector<uint8_t>& code, const WasmValType& res,
                             FuncState& fs, int depth, const WasmModule& m) {
    if (!has_gc_) {
      return false;
    }
    if (res.kind == WasmType::kI32) {
      const size_t k = r_.Index(7);
      if (k == 0) {
        // `i31.get_s` or `i31.get_u`.
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        EmitGcOp(code, 0x1C);                     // `ref.i31`.
        EmitGcOp(code, r_.Coin() ? 0x1D : 0x1E);  // `i31.get_s` / `i31.get_u`.
        return true;
      }
      if (k == 1) {
        // `struct.get` or packed `struct.get_s/u` on `$S_base`.
        EmitNonNullStructExpr(code, r_.Coin() ? s_base_def_ : s_sub_def_, fs,
                              depth - 1, m);
        if (r_.Coin()) {
          EmitGcOp(code, 0x02);  // `struct.get $S_base 0` (`i32`).
          EncodeU32(code, s_base_idx_);
          EncodeU32(code, 0);
        } else {
          // Field 1 of `$S_base` is packed `i8` or `i16`.
          EmitGcOp(code, r_.Coin() ? 0x03 : 0x04);  // `struct.get_s/u`.
          EncodeU32(code, s_base_idx_);
          EncodeU32(code, 1);
        }
        return true;
      }
      if (k == 2) {
        // `array.len`.
        EmitNonNullArrayExpr(code, r_.Coin() ? a_num_def_ : a_ref_def_, fs,
                             depth - 1, m);
        EmitGcOp(code, 0x0F);  // `array.len`.
        return true;
      }
      if (k == 3) {
        // `ref.test` or `ref.test null` on `anyref` against concrete or
        // abstract target types.
        BuildExpr(code, WasmType::kAnyRef, fs, depth - 1, m);
        EmitGcOp(code, r_.Coin() ? 0x14 : 0x15);
        const size_t tpick = r_.Index(6);
        if (tpick == 0) {
          EncodeS33(code, s_base_idx_);
        } else if (tpick == 1) {
          EncodeS33(code, s_sub_idx_);
        } else if (tpick == 2) {
          EncodeS33(code, a_num_idx_);
        } else if (tpick == 3) {
          code.push_back(static_cast<uint8_t>(WasmType::kI31Ref));
        } else if (tpick == 4) {
          code.push_back(static_cast<uint8_t>(WasmType::kStructRef));
        } else {
          code.push_back(static_cast<uint8_t>(WasmType::kArrayRef));
        }
        return true;
      }
      if (k == 4) {
        // `ref.eq` on two `eqref` values.
        BuildExpr(code, WasmType::kEqRef, fs, depth - 1, m);
        BuildExpr(code, WasmType::kEqRef, fs, depth - 1, m);
        code.push_back(0xD3);  // `ref.eq`.
        return true;
      }
      if (k == 5) {
        // `ref.is_null`.
        BuildExpr(code, WasmType::kAnyRef, fs, depth - 1, m);
        code.push_back(0xD1);  // `ref.is_null`.
        return true;
      }
      if (a_num_def_.elem.Unpacked().kind == WasmType::kI32) {
        // `array.get` / `array.get_s` / `array.get_u` on `$A_num`.
        EmitNonNullArrayExpr(code, a_num_def_, fs, depth - 1, m);
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        code.push_back(0x41);
        EncodeI32(code, 3);
        code.push_back(0x71);
        if (a_num_def_.elem.packed) {
          EmitGcOp(code, r_.Coin() ? 0x0C : 0x0D);
        } else {
          EmitGcOp(code, 0x0B);
        }
        EncodeU32(code, a_num_idx_);
        return true;
      }
    }

    // Match `array.get` on `$A_num` for `i64`/`f32`/`f64`.
    if (a_num_def_.elem.Unpacked() == res && !a_num_def_.elem.packed) {
      EmitNonNullArrayExpr(code, a_num_def_, fs, depth - 1, m);
      BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
      code.push_back(0x41);
      EncodeI32(code, 3);
      code.push_back(0x71);
      EmitGcOp(code, 0x0B);  // `array.get`.
      EncodeU32(code, a_num_idx_);
      return true;
    }

    // Match `struct.get` on `$S_base` field 2 or `$S_alt` field 4 (`f32`).
    if (s_base_def_.fields.size() >= 3 && !s_base_def_.fields[2].type.packed &&
        s_base_def_.fields[2].type.Unpacked() == res) {
      EmitNonNullStructExpr(code, s_base_def_, fs, depth - 1, m);
      EmitGcOp(code, 0x02);
      EncodeU32(code, s_base_idx_);
      EncodeU32(code, 2);
      return true;
    }
    if (res.kind == WasmType::kF32 && s_alt_def_.fields.size() >= 5) {
      EmitNonNullStructExpr(code, s_alt_def_, fs, depth - 1, m);
      EmitGcOp(code, 0x02);
      EncodeU32(code, s_alt_idx_);
      EncodeU32(code, static_cast<uint32_t>(s_alt_def_.fields.size() - 1));
      return true;
    }
    return false;
  }

  void EmitUnaryOp(std::vector<uint8_t>& code, const WasmValType& res,
                   FuncState& fs, int depth, const WasmModule& m) {
    if (res.kind == WasmType::kI32 && r_.Chance(0.35)) {
      if (r_.Coin()) {
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        code.push_back(0x45);  // `i32.eqz`.
      } else {
        BuildExpr(code, WasmType::kI64, fs, depth - 1, m);
        code.push_back(0x50);  // `i64.eqz`.
      }
      return;
    }
    BuildExpr(code, res, fs, depth - 1, m);
    if (res.kind == WasmType::kI32) {
      static constexpr uint8_t kOps[] = {0x67, 0x68, 0x69, 0xC0, 0xC1};
      code.push_back(kOps[r_.Index(5)]);
    } else if (res.kind == WasmType::kI64) {
      static constexpr uint8_t kOps[] = {0x79, 0x7A, 0x7B, 0xC2, 0xC3, 0xC4};
      code.push_back(kOps[r_.Index(6)]);
    } else if (res.kind == WasmType::kF32) {
      static constexpr uint8_t kOps[] = {0x8B, 0x8C, 0x8D, 0x8E,
                                         0x8F, 0x90, 0x91};
      code.push_back(kOps[r_.Index(7)]);
    } else {
      static constexpr uint8_t kOps[] = {0x99, 0x9A, 0x9B, 0x9C,
                                         0x9D, 0x9E, 0x9F};
      code.push_back(kOps[r_.Index(7)]);
    }
  }

  void EmitBinaryOp(std::vector<uint8_t>& code, const WasmValType& res,
                    FuncState& fs, int depth, const WasmModule& m) {
    if (res.kind == WasmType::kI32 && r_.Chance(0.35)) {
      WasmValType arg_type = RandomScalarType();
      BuildExpr(code, arg_type, fs, depth - 1, m);
      BuildExpr(code, arg_type, fs, depth - 1, m);
      if (arg_type.kind == WasmType::kI32) {
        code.push_back(static_cast<uint8_t>(0x46 + r_.Index(10)));
      } else if (arg_type.kind == WasmType::kI64) {
        code.push_back(static_cast<uint8_t>(0x51 + r_.Index(10)));
      } else if (arg_type.kind == WasmType::kF32) {
        code.push_back(static_cast<uint8_t>(0x5B + r_.Index(6)));
      } else {
        code.push_back(static_cast<uint8_t>(0x61 + r_.Index(6)));
      }
      return;
    }

    BuildExpr(code, res, fs, depth - 1, m);
    BuildExpr(code, res, fs, depth - 1, m);

    if (res.kind == WasmType::kI32) {
      if (r_.Chance(0.2)) {
        if (!fs.is_start && r_.Chance(kWasmTrapDivProb())) {
          code.push_back(r_.Coin() ? 0x6D : 0x6F);  // `i32.div_s` / `rem_s`.
        } else {
          code.push_back(0x41);
          EncodeI32(code, 1);
          code.push_back(0x72);                     // `i32.or 1`.
          code.push_back(r_.Coin() ? 0x6E : 0x70);  // `i32.div_u` / `rem_u`.
        }
      } else {
        static constexpr uint8_t kOps[] = {0x6A, 0x6B, 0x6C, 0x71, 0x72, 0x73,
                                           0x74, 0x75, 0x76, 0x77, 0x78};
        code.push_back(kOps[r_.Index(11)]);
      }
    } else if (res.kind == WasmType::kI64) {
      if (r_.Chance(0.2)) {
        if (!fs.is_start && r_.Chance(kWasmTrapDivProb())) {
          code.push_back(r_.Coin() ? 0x7F : 0x81);  // `i64.div_s` / `rem_s`.
        } else {
          code.push_back(0x42);
          EncodeI64(code, 1);
          code.push_back(0x84);                     // `i64.or 1`.
          code.push_back(r_.Coin() ? 0x80 : 0x82);  // `i64.div_u` / `rem_u`.
        }
      } else {
        static constexpr uint8_t kOps[] = {0x7C, 0x7D, 0x7E, 0x83, 0x84, 0x85,
                                           0x86, 0x87, 0x88, 0x89, 0x8A};
        code.push_back(kOps[r_.Index(11)]);
      }
    } else if (res.kind == WasmType::kF32) {
      static constexpr uint8_t kOps[] = {0x92, 0x93, 0x94, 0x95,
                                         0x96, 0x97, 0x98};
      code.push_back(kOps[r_.Index(7)]);
    } else {
      static constexpr uint8_t kOps[] = {0xA0, 0xA1, 0xA2, 0xA3,
                                         0xA4, 0xA5, 0xA6};
      code.push_back(kOps[r_.Index(7)]);
    }
  }

  void EmitConversion(std::vector<uint8_t>& code, const WasmValType& res,
                      FuncState& fs, int depth, const WasmModule& m) {
    WasmValType src = RandomScalarType();
    if (src == res) {
      static constexpr WasmType kOthers[4][3] = {
          {WasmType::kI64, WasmType::kF32, WasmType::kF64},
          {WasmType::kI32, WasmType::kF32, WasmType::kF64},
          {WasmType::kI32, WasmType::kI64, WasmType::kF64},
          {WasmType::kI32, WasmType::kI64, WasmType::kF32},
      };
      const size_t idx = res.kind == WasmType::kI32   ? 0
                         : res.kind == WasmType::kI64 ? 1
                         : res.kind == WasmType::kF32 ? 2
                                                      : 3;
      src = kOthers[idx][r_.Index(3)];
    }
    BuildExpr(code, src, fs, depth - 1, m);

    if (res.kind == WasmType::kI32 && src.kind == WasmType::kI64) {
      code.push_back(0xA7);  // `i32.wrap_i64`.
    } else if (res.kind == WasmType::kI32 && src.kind == WasmType::kF32) {
      if (r_.Chance(0.35)) {
        code.push_back(0xBC);  // `i32.reinterpret_f32`.
      } else if (!fs.is_start && r_.Chance(kWasmTrapConvertProb())) {
        code.push_back(r_.Coin() ? 0xA8 : 0xA9);
      } else {
        EmitNumericOp(code, r_.Coin() ? 0 : 1);  // `i32.trunc_sat_f32_s/u`.
      }
    } else if (res.kind == WasmType::kI32 && src.kind == WasmType::kF64) {
      if (!fs.is_start && r_.Chance(kWasmTrapConvertProb())) {
        code.push_back(r_.Coin() ? 0xAA : 0xAB);
      } else {
        EmitNumericOp(code, r_.Coin() ? 2 : 3);  // `i32.trunc_sat_f64_s/u`.
      }
    } else if (res.kind == WasmType::kI64 && src.kind == WasmType::kI32) {
      code.push_back(r_.Coin() ? 0xAC : 0xAD);  // `i64.extend_i32_s/u`.
    } else if (res.kind == WasmType::kI64 && src.kind == WasmType::kF32) {
      if (!fs.is_start && r_.Chance(kWasmTrapConvertProb())) {
        code.push_back(r_.Coin() ? 0xAE : 0xAF);
      } else {
        EmitNumericOp(code, r_.Coin() ? 4 : 5);  // `i64.trunc_sat_f32_s/u`.
      }
    } else if (res.kind == WasmType::kI64 && src.kind == WasmType::kF64) {
      if (r_.Chance(0.35)) {
        code.push_back(0xBD);  // `i64.reinterpret_f64`.
      } else if (!fs.is_start && r_.Chance(kWasmTrapConvertProb())) {
        code.push_back(r_.Coin() ? 0xB0 : 0xB1);
      } else {
        EmitNumericOp(code, r_.Coin() ? 6 : 7);  // `i64.trunc_sat_f64_s/u`.
      }
    } else if (res.kind == WasmType::kF32 && src.kind == WasmType::kI32) {
      static constexpr uint8_t kOps[] = {0xB2, 0xB3, 0xBE};
      code.push_back(kOps[r_.Index(3)]);
    } else if (res.kind == WasmType::kF32 && src.kind == WasmType::kI64) {
      code.push_back(r_.Coin() ? 0xB4 : 0xB5);
    } else if (res.kind == WasmType::kF32 && src.kind == WasmType::kF64) {
      code.push_back(0xB6);  // `f32.demote_f64`.
    } else if (res.kind == WasmType::kF64 && src.kind == WasmType::kI32) {
      code.push_back(r_.Coin() ? 0xB7 : 0xB8);
    } else if (res.kind == WasmType::kF64 && src.kind == WasmType::kI64) {
      static constexpr uint8_t kOps[] = {0xB9, 0xBA, 0xBF};
      code.push_back(kOps[r_.Index(3)]);
    } else {
      code.push_back(0xBB);  // `f64.promote_f32`.
    }
  }

  void BuildExpr(std::vector<uint8_t>& code, const WasmValType& res,
                 FuncState& fs, int depth, const WasmModule& m) {
    if (res.kind == WasmType::kV128) {
      EmitSimdExpr(code, fs, depth, m);
      return;
    }
    if (res.IsRef()) {
      EmitGcRefExpr(code, res, fs, depth, m);
      return;
    }
    if (depth <= 0 || fs.budget <= 0) {
      EmitConst(code, res);
      return;
    }
    --fs.budget;

    const size_t choice = r_.Index(14);
    switch (choice) {
      case 0: {  // Constant.
        EmitConst(code, res);
        return;
      }
      case 1: {  // `local.get`.
        std::vector<size_t> cands;
        for (size_t i = 0; i < fs.user_locals.size(); ++i) {
          if (fs.user_locals[i] == res) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          code.push_back(0x20);
          EncodeU32(code, static_cast<uint32_t>(r_.Pick(cands)));
          return;
        }
        break;
      }
      case 2: {  // `local.tee`.
        std::vector<size_t> cands;
        for (size_t i = 0; i < fs.user_locals.size(); ++i) {
          if (fs.user_locals[i] == res) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          BuildExpr(code, res, fs, depth - 1, m);
          code.push_back(0x22);
          EncodeU32(code, static_cast<uint32_t>(r_.Pick(cands)));
          return;
        }
        break;
      }
      case 3: {  // `global.get`.
        std::vector<size_t> cands;
        for (size_t i = 0; i < globals_.size(); ++i) {
          if (globals_[i].type == res) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          code.push_back(0x23);
          EncodeU32(code, static_cast<uint32_t>(r_.Pick(cands)));
          return;
        }
        break;
      }
      case 4: {  // Unary numeric operator.
        EmitUnaryOp(code, res, fs, depth, m);
        return;
      }
      case 5: {  // Binary numeric operator or comparison.
        EmitBinaryOp(code, res, fs, depth, m);
        return;
      }
      case 6: {  // Numeric conversion, reinterpret, or saturating truncation.
        EmitConversion(code, res, fs, depth, m);
        return;
      }
      case 7: {  // Masked memory load, atomic load/RMW, or `memory.size`.
        if (m.memory) {
          EmitLoadExpr(code, res, fs, depth, m);
          return;
        }
        break;
      }
      case 8: {  // `select`.
        BuildExpr(code, res, fs, depth - 1, m);
        BuildExpr(code, res, fs, depth - 1, m);
        BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
        code.push_back(0x1B);
        return;
      }
      case 9: {  // Valued `if`/`else` or valued `block`.
        if (r_.Coin()) {
          BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
          fs.block_stack.push_back({false, true, res});
          code.push_back(0x04);
          code.push_back(static_cast<uint8_t>(res.kind));
          BuildExpr(code, res, fs, depth - 1, m);
          code.push_back(0x05);
          BuildExpr(code, res, fs, depth - 1, m);
          code.push_back(0x0B);
          fs.block_stack.pop_back();
        } else {
          fs.block_stack.push_back({false, true, res});
          code.push_back(0x02);
          code.push_back(static_cast<uint8_t>(res.kind));
          BuildExpr(code, res, fs, depth - 1, m);
          code.push_back(0x0B);
          fs.block_stack.pop_back();
        }
        return;
      }
      case 10: {  // Forward `br_if` to a valued non-loop label of type `res`.
        std::vector<size_t> cands;
        for (size_t i = 0; i < fs.block_stack.size(); ++i) {
          if (!fs.block_stack[i].is_loop && fs.block_stack[i].has_result &&
              fs.block_stack[i].result == res) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          const size_t target_idx = r_.Pick(cands);
          const uint32_t rel_depth =
              static_cast<uint32_t>(fs.block_stack.size() - 1 - target_idx);
          BuildExpr(code, res, fs, depth - 1, m);
          BuildExpr(code, WasmType::kI32, fs, depth - 1, m);
          code.push_back(0x0D);
          EncodeU32(code, rel_depth);
          return;
        }
        break;
      }
      case 11: {  // Direct `call` or `call_indirect` returning `res`.
        if (!fs.allow_calls) {
          break;
        }
        if (m.table && table_callees_ > 0 && fs.my_def_idx >= table_callees_ &&
            !callee_sig_.results.empty() && callee_sig_.results[0] == res &&
            r_.Coin()) {
          EmitCallIndirect(code, fs, depth, m, /*use_param0=*/r_.Coin());
          return;
        }
        std::vector<size_t> cands;
        for (size_t i = 0; i < fs.my_func; ++i) {
          if (i >= m.imports.size() && i < total_imported_funcs_) {
            continue;
          }
          if (static_cast<int>(i) == m.jspi_import &&
              static_cast<int>(fs.my_def_idx) != m.jspi_export) {
            continue;
          }
          if (m.jspi_export >= 0 &&
              i == total_imported_funcs_ + static_cast<size_t>(m.jspi_export)) {
            continue;
          }
          if (!all_func_sigs_[i].results.empty() &&
              all_func_sigs_[i].results[0] == res) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          const size_t callee = r_.Pick(cands);
          for (const WasmValType& p : all_func_sigs_[callee].params) {
            BuildExpr(code, p, fs, depth - 1, m);
          }
          code.push_back(0x10);
          EncodeU32(code, static_cast<uint32_t>(callee));
          return;
        }
        break;
      }
      case 12: {  // GC read/test/length/i31 feeding into a scalar value.
        if (has_gc_ && TryEmitGcToScalarExpr(code, res, fs, depth, m)) {
          return;
        }
        break;
      }
      case 13: {  // SIMD extract-lane / bitmask / all_true into a scalar.
        if (has_simd_) {
          EmitSimdToScalarExpr(code, res, fs, depth, m);
          return;
        }
        break;
      }
    }
    EmitConst(code, res);
  }
};

}  // namespace

WasmModule RandomWasmModule(Random& r) {
  WasmBuilder builder(r);
  return builder.Build();
}

}  // namespace redline
