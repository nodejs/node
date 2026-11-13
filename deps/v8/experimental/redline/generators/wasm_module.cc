// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/generators/wasm_module.h"

#include <cstring>
#include <string>

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
REDLINE_TUNABLE(kWasmTrapMemoryProb,
                "generators.WasmModule.trap_memory_probability", 0.05, 0, 1,
                "probability of unmasked memory access");
REDLINE_TUNABLE(kWasmTrapDivProb, "generators.WasmModule.trap_div_probability",
                0.05, 0, 1, "probability of non-guarded div/rem");
REDLINE_TUNABLE(kWasmTrapConvertProb,
                "generators.WasmModule.trap_convert_probability", 0.05, 0, 1,
                "probability of non-saturating convert");

namespace {

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

class WasmBuilder {
 public:
  explicit WasmBuilder(Random& r) : r_(r) {}

  WasmModule Build() {
    WasmModule m;

    size_t num_imports =
        r_.Index(static_cast<size_t>(kWasmMaxImports.AsInt()) + 1);
    size_t num_funcs =
        1 + r_.Index(static_cast<size_t>(kWasmMaxFunctions.AsInt()));
    m.memory = r_.Chance(kWasmMemoryProb());
    m.table = r_.Chance(kWasmTableProb());
    size_t num_globals =
        r_.Index(static_cast<size_t>(kWasmMaxGlobals.AsInt()) + 1);

    for (size_t i = 0; i < num_imports; ++i) {
      WasmSignature sig;
      size_t params = r_.Index(static_cast<size_t>(kWasmMaxParams.AsInt()) + 1);
      for (size_t p = 0; p < params; ++p) {
        sig.params.push_back(RandomTypeNoI64());
      }
      if (r_.Coin()) {
        sig.results.push_back(RandomTypeNoI64());
      }
      m.imports.push_back(sig);
      sigs_.push_back(sig);
    }

    for (size_t i = 0; i < num_funcs; ++i) {
      WasmSignature sig;
      size_t params = r_.Index(static_cast<size_t>(kWasmMaxParams.AsInt()) + 1);
      for (size_t p = 0; p < params; ++p) {
        sig.params.push_back(RandomType());
      }
      if (r_.Coin()) {
        sig.results.push_back(RandomType());
      }
      m.functions.push_back(sig);
      sigs_.push_back(sig);
    }

    for (size_t i = 0; i < num_globals; ++i) {
      WasmType type = RandomType();
      bool mut = r_.Coin();
      globals_.push_back({type, mut});
      if (mut) {
        m.globals.push_back(type);
      }
    }

    std::vector<uint8_t>& b = m.bytes;
    b.insert(b.end(), {0x00, 0x61, 0x73, 0x6D});
    b.insert(b.end(), {0x01, 0x00, 0x00, 0x00});

    b.push_back(1);
    std::vector<uint8_t> sec_types;
    EncodeU32(sec_types, static_cast<uint32_t>(sigs_.size()));
    for (const auto& sig : sigs_) {
      sec_types.push_back(0x60);
      EncodeU32(sec_types, static_cast<uint32_t>(sig.params.size()));
      for (auto t : sig.params) {
        sec_types.push_back(static_cast<uint8_t>(t));
      }
      EncodeU32(sec_types, static_cast<uint32_t>(sig.results.size()));
      for (auto t : sig.results) {
        sec_types.push_back(static_cast<uint8_t>(t));
      }
    }
    EncodeU32(b, static_cast<uint32_t>(sec_types.size()));
    b.insert(b.end(), sec_types.begin(), sec_types.end());

    if (num_imports > 0) {
      b.push_back(2);
      std::vector<uint8_t> sec;
      EncodeU32(sec, static_cast<uint32_t>(num_imports));
      for (size_t i = 0; i < num_imports; ++i) {
        EncodeString(sec, "m");
        EncodeString(sec, "f" + std::to_string(i));
        sec.push_back(0x00);
        EncodeU32(sec, static_cast<uint32_t>(i));
      }
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    if (num_funcs > 0) {
      b.push_back(3);
      std::vector<uint8_t> sec;
      EncodeU32(sec, static_cast<uint32_t>(num_funcs));
      for (size_t i = 0; i < num_funcs; ++i) {
        EncodeU32(sec, static_cast<uint32_t>(num_imports + i));
      }
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    if (m.table) {
      b.push_back(4);
      std::vector<uint8_t> sec;
      EncodeU32(sec, 1);
      sec.push_back(0x70);
      sec.push_back(0x00);
      EncodeU32(sec, static_cast<uint32_t>(num_funcs));
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    if (m.memory) {
      b.push_back(5);
      std::vector<uint8_t> sec;
      EncodeU32(sec, 1);
      sec.push_back(0x00);
      mem_pages_ =
          1 + r_.Index(static_cast<size_t>(kWasmMaxMemoryPages.AsInt()));
      EncodeU32(sec, static_cast<uint32_t>(mem_pages_));
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    if (num_globals > 0) {
      b.push_back(6);
      std::vector<uint8_t> sec;
      EncodeU32(sec, static_cast<uint32_t>(num_globals));
      for (const auto& g : globals_) {
        sec.push_back(static_cast<uint8_t>(g.type));
        sec.push_back(g.mut ? 0x01 : 0x00);
        EmitConst(sec, g.type);
        sec.push_back(0x0B);
      }
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    {
      b.push_back(7);
      std::vector<uint8_t> sec;
      uint32_t num_exports =
          static_cast<uint32_t>(num_funcs + (m.memory ? 1 : 0) +
                                (m.table ? 1 : 0) + m.globals.size());
      EncodeU32(sec, num_exports);
      for (size_t i = 0; i < num_funcs; ++i) {
        EncodeString(sec, "e" + std::to_string(i));
        sec.push_back(0x00);
        EncodeU32(sec, static_cast<uint32_t>(num_imports + i));
      }
      if (m.memory) {
        EncodeString(sec, "mem");
        sec.push_back(0x02);
        EncodeU32(sec, 0);
      }
      if (m.table) {
        EncodeString(sec, "tab");
        sec.push_back(0x01);
        EncodeU32(sec, 0);
      }
      size_t g_idx = 0;
      size_t g_exp = 0;
      for (const auto& g : globals_) {
        if (g.mut) {
          EncodeString(sec, "g" + std::to_string(g_exp++));
          sec.push_back(0x03);
          EncodeU32(sec, static_cast<uint32_t>(g_idx));
        }
        g_idx++;
      }
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    if (m.table && num_funcs > 0) {
      b.push_back(9);
      std::vector<uint8_t> sec;
      EncodeU32(sec, 1);
      sec.push_back(0x00);
      sec.push_back(0x41);
      EncodeI32(sec, 0);
      sec.push_back(0x0B);
      EncodeU32(sec, static_cast<uint32_t>(num_funcs));
      for (size_t i = 0; i < num_funcs; ++i) {
        EncodeU32(sec, static_cast<uint32_t>(num_imports + i));
      }
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    if (num_funcs > 0) {
      b.push_back(10);
      std::vector<uint8_t> sec;
      EncodeU32(sec, static_cast<uint32_t>(num_funcs));
      for (size_t i = 0; i < num_funcs; ++i) {
        std::vector<uint8_t> code;
        size_t num_locals =
            r_.Index(static_cast<size_t>(kWasmMaxLocals.AsInt()) + 1);
        std::vector<WasmType> locals;
        locals.reserve(num_locals);
        for (size_t l = 0; l < num_locals; ++l) {
          locals.push_back(RandomType());
        }
        EncodeU32(code, static_cast<uint32_t>(locals.size()));
        for (auto t : locals) {
          EncodeU32(code, 1);
          code.push_back(static_cast<uint8_t>(t));
        }

        std::vector<WasmType> all_locals = m.functions[i].params;
        all_locals.insert(all_locals.end(), locals.begin(), locals.end());

        WasmType res = m.functions[i].results.empty()
                           ? WasmType::kI32
                           : m.functions[i].results[0];

        std::vector<WasmType> block_stack;
        if (!m.functions[i].results.empty()) {
          block_stack.push_back(res);
        }

        BuildExpr(code, res, all_locals, kWasmMaxExprDepth.AsInt(),
                  num_imports + i, block_stack, m);
        if (m.functions[i].results.empty()) {
          code.push_back(0x1A);
        }

        code.push_back(0x0B);

        EncodeU32(sec, static_cast<uint32_t>(code.size()));
        sec.insert(sec.end(), code.begin(), code.end());
      }
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    if (m.memory) {
      b.push_back(11);
      std::vector<uint8_t> sec;
      EncodeU32(sec, 1);
      sec.push_back(0x00);
      sec.push_back(0x41);
      EncodeI32(sec, 0);
      sec.push_back(0x0B);
      std::vector<uint8_t> data(16, 0x42);
      EncodeU32(sec, static_cast<uint32_t>(data.size()));
      sec.insert(sec.end(), data.begin(), data.end());
      EncodeU32(b, static_cast<uint32_t>(sec.size()));
      b.insert(b.end(), sec.begin(), sec.end());
    }

    return m;
  }

 private:
  Random& r_;
  std::vector<WasmSignature> sigs_;
  struct Global {
    WasmType type;
    bool mut;
  };
  std::vector<Global> globals_;
  size_t mem_pages_ = 0;

  WasmType RandomType() {
    size_t t = r_.Index(4);
    if (t == 0) {
      return WasmType::kI32;
    }
    if (t == 1) {
      return WasmType::kI64;
    }
    if (t == 2) {
      return WasmType::kF32;
    }
    return WasmType::kF64;
  }

  WasmType RandomTypeNoI64() {
    size_t t = r_.Index(3);
    if (t == 0) {
      return WasmType::kI32;
    }
    if (t == 1) {
      return WasmType::kF32;
    }
    return WasmType::kF64;
  }

  void EmitConst(std::vector<uint8_t>& code, WasmType type) {
    if (type == WasmType::kI32) {
      code.push_back(0x41);
      EncodeI32(code, static_cast<int32_t>(r_.Range(-1000, 1000)));
    } else if (type == WasmType::kI64) {
      code.push_back(0x42);
      EncodeI64(code, r_.Range(-1000, 1000));
    } else if (type == WasmType::kF32) {
      code.push_back(0x43);
      EncodeF32(code, static_cast<float>(r_.Double() * 100.0));
    } else {
      code.push_back(0x44);
      EncodeF64(code, r_.Double() * 100.0);
    }
  }

  void BuildExpr(std::vector<uint8_t>& code, WasmType res,
                 const std::vector<WasmType>& locals, int depth, size_t my_func,
                 std::vector<WasmType>& block_stack, const WasmModule& m) {
    if (depth <= 0) {
      EmitConst(code, res);
      return;
    }
    size_t choice = r_.Index(10);
    switch (choice) {
      default:
        break;
      case 1: {
        std::vector<size_t> cands;
        for (size_t i = 0; i < locals.size(); ++i) {
          if (locals[i] == res) {
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
      case 2: {
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
      case 3: {
        WasmType arg_type = res;
        if (res == WasmType::kI32 && r_.Coin()) {
          arg_type = RandomType();
        }
        BuildExpr(code, arg_type, locals, depth - 1, my_func, block_stack, m);
        BuildExpr(code, arg_type, locals, depth - 1, my_func, block_stack, m);

        if (res == arg_type && r_.Chance(0.2)) {
          if (!r_.Chance(kWasmTrapDivProb())) {
            if (arg_type == WasmType::kI32) {
              code.push_back(0x41);
              EncodeI32(code, 1);
              code.push_back(0x72);  // i32.or 1 (never 0)
              code.push_back(0x6E);  // i32.div_u
            } else if (arg_type == WasmType::kI64) {
              code.push_back(0x42);
              EncodeI64(code, 1);
              code.push_back(0x84);  // i64.or 1 (never 0)
              code.push_back(0x80);  // i64.div_u
            } else if (arg_type == WasmType::kF32) {
              code.push_back(0x43);
              EncodeF32(code, 1.0f);
              code.push_back(0x92);
              code.push_back(0x95);  // f32.div
            } else {
              code.push_back(0x44);
              EncodeF64(code, 1.0);
              code.push_back(0xA0);
              code.push_back(0xA3);  // f64.div
            }
          } else {
            if (arg_type == WasmType::kI32) {
              code.push_back(0x6D);  // i32.div_s
            } else if (arg_type == WasmType::kI64) {
              code.push_back(0x7F);  // i64.div_s
            } else if (arg_type == WasmType::kF32) {
              code.push_back(0x95);  // f32.div
            } else {
              code.push_back(0xA3);  // f64.div
            }
          }
        } else {
          if (arg_type == WasmType::kI32) {
            if (res == WasmType::kI32) {
              code.push_back(r_.Coin() ? 0x6A : 0x6B);
            } else {
              code.push_back(r_.Coin() ? 0x46 : 0x47);
            }
          } else if (arg_type == WasmType::kI64) {
            if (res == WasmType::kI64) {
              code.push_back(r_.Coin() ? 0x7C : 0x7D);
            } else {
              code.push_back(r_.Coin() ? 0x51 : 0x52);
            }
          } else if (arg_type == WasmType::kF32) {
            if (res == WasmType::kF32) {
              code.push_back(r_.Coin() ? 0x92 : 0x93);
            } else {
              code.push_back(r_.Coin() ? 0x5B : 0x5C);
            }
          } else {
            if (res == WasmType::kF64) {
              code.push_back(r_.Coin() ? 0xA0 : 0xA1);
            } else {
              code.push_back(r_.Coin() ? 0x61 : 0x62);
            }
          }
        }
        return;
      }
      case 4: {
        std::vector<size_t> cands;
        for (size_t i = 0; i < sigs_.size(); ++i) {
          if (i < my_func &&
              ((sigs_[i].results.empty() && res == WasmType::kI32) ||
               (!sigs_[i].results.empty() && sigs_[i].results[0] == res))) {
            cands.push_back(i);
          }
        }
        if (!cands.empty()) {
          size_t callee = r_.Pick(cands);
          for (auto t : sigs_[callee].params) {
            BuildExpr(code, t, locals, depth - 1, my_func, block_stack, m);
          }
          code.push_back(0x10);
          EncodeU32(code, static_cast<uint32_t>(callee));
          if (sigs_[callee].results.empty()) {
            EmitConst(code, res);
          }
          return;
        }
        break;
      }
      case 5: {
        WasmType arg_type = RandomType();
        BuildExpr(code, arg_type, locals, depth - 1, my_func, block_stack, m);
        if (res == WasmType::kI32 && arg_type == WasmType::kI64) {
          code.push_back(0xA7);
        } else if (res == WasmType::kI32 && arg_type == WasmType::kF32) {
          if (r_.Chance(kWasmTrapConvertProb())) {
            code.push_back(0xA8);
          } else {
            code.push_back(0xFC);
            EncodeU32(code, 0);
          }
        } else if (res == WasmType::kI32 && arg_type == WasmType::kF64) {
          if (r_.Chance(kWasmTrapConvertProb())) {
            code.push_back(0xAA);
          } else {
            code.push_back(0xFC);
            EncodeU32(code, 2);
          }
        } else if (res == WasmType::kI64 && arg_type == WasmType::kI32) {
          code.push_back(0xAC);
        } else if (res == WasmType::kI64 && arg_type == WasmType::kF32) {
          if (r_.Chance(kWasmTrapConvertProb())) {
            code.push_back(0xAE);
          } else {
            code.push_back(0xFC);
            EncodeU32(code, 4);
          }
        } else if (res == WasmType::kI64 && arg_type == WasmType::kF64) {
          if (r_.Chance(kWasmTrapConvertProb())) {
            code.push_back(0xB0);
          } else {
            code.push_back(0xFC);
            EncodeU32(code, 6);
          }
        } else if (res == WasmType::kF32 && arg_type == WasmType::kI32) {
          code.push_back(0xB2);
        } else if (res == WasmType::kF32 && arg_type == WasmType::kI64) {
          code.push_back(0xB4);
        } else if (res == WasmType::kF32 && arg_type == WasmType::kF64) {
          code.push_back(0xB6);
        } else if (res == WasmType::kF64 && arg_type == WasmType::kI32) {
          code.push_back(0xB7);
        } else if (res == WasmType::kF64 && arg_type == WasmType::kI64) {
          code.push_back(0xB9);
        } else if (res == WasmType::kF64 && arg_type == WasmType::kF32) {
          code.push_back(0xBB);
        } else {
          code.push_back(0x1A);
          EmitConst(code, res);
        }
        return;
      }
      case 6: {
        if (m.memory) {
          BuildExpr(code, WasmType::kI32, locals, depth - 1, my_func,
                    block_stack, m);
          if (!r_.Chance(kWasmTrapMemoryProb())) {
            code.push_back(0x41);
            EncodeI32(code, static_cast<int32_t>((mem_pages_ * 65536) - 8));
            code.push_back(0x71);
          }
          if (res == WasmType::kI32) {
            code.push_back(0x28);
            EncodeU32(code, 2);
            EncodeU32(code, 0);
          } else if (res == WasmType::kI64) {
            code.push_back(0x29);
            EncodeU32(code, 3);
            EncodeU32(code, 0);
          } else if (res == WasmType::kF32) {
            code.push_back(0x2A);
            EncodeU32(code, 2);
            EncodeU32(code, 0);
          } else {
            code.push_back(0x2B);
            EncodeU32(code, 3);
            EncodeU32(code, 0);
          }
          return;
        }
        break;
      }
      case 7: {
        block_stack.push_back(res);
        code.push_back(0x02);
        code.push_back(static_cast<uint8_t>(res));
        BuildExpr(code, res, locals, depth - 1, my_func, block_stack, m);
        code.push_back(0x0B);
        block_stack.pop_back();
        return;
      }
      case 8: {
        BuildExpr(code, WasmType::kI32, locals, depth - 1, my_func, block_stack,
                  m);
        block_stack.push_back(res);
        code.push_back(0x04);
        code.push_back(static_cast<uint8_t>(res));
        BuildExpr(code, res, locals, depth - 1, my_func, block_stack, m);
        code.push_back(0x05);
        BuildExpr(code, res, locals, depth - 1, my_func, block_stack, m);
        code.push_back(0x0B);
        block_stack.pop_back();
        return;
      }
      case 9: {
        if (!block_stack.empty()) {
          std::vector<size_t> cands;
          for (size_t i = 0; i < block_stack.size(); ++i) {
            if (block_stack[block_stack.size() - 1 - i] == res) {
              cands.push_back(i);
            }
          }
          if (!cands.empty()) {
            BuildExpr(code, res, locals, depth - 1, my_func, block_stack, m);
            BuildExpr(code, WasmType::kI32, locals, depth - 1, my_func,
                      block_stack, m);
            code.push_back(0x0D);
            EncodeU32(code, static_cast<uint32_t>(r_.Pick(cands)));
            return;
          }
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
