// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The C++ style guide recommends using <re2> instead of <regex>. However, the
// former isn't available in V8.
#include <regex>  // NOLINT(build/c++11)

#include "src/api-inl.h"
#include "src/disassembler.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

std::string DisassembleFunction(const char* function) {
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  Handle<JSFunction> f = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(context, v8_str(function)).ToLocalChecked())));

  Address begin = f->code()->raw_instruction_start();
  Address end = f->code()->raw_instruction_end();
  Isolate* isolate = CcTest::i_isolate();
  std::ostringstream os;
  Disassembler::Decode(isolate, &os, reinterpret_cast<byte*>(begin),
                       reinterpret_cast<byte*>(end),
                       CodeReference(handle(f->code(), isolate)));
  return os.str();
}

struct Matchers {
  std::string start = "0x[0-9a-f]+ +[0-9a-f]+ +[0-9a-f]+ +";
  std::regex map_load_re =
      std::regex(start + "ldr r([0-9]+), \\[r([0-9]+), #-1\\]");
  std::regex load_const_re = std::regex(start + "ldr r([0-9]+), \\[pc, .*");
  std::regex cmp_re = std::regex(start + "cmp r([0-9]+), r([0-9]+)");
  std::regex bne_re = std::regex(start + "bne (.*)");
  std::regex beq_re = std::regex(start + "beq (.*)");
  std::regex b_re = std::regex(start + "b (.*)");
  std::regex eorne_re =
      std::regex(start + "eorne r([0-9]+), r([0-9]+), r([0-9]+)");
  std::regex eoreq_re =
      std::regex(start + "eoreq r([0-9]+), r([0-9]+), r([0-9]+)");
  std::regex csdb_re = std::regex(start + "csdb");
  std::regex load_field_re =
      std::regex(start + "ldr r([0-9]+), \\[r([0-9]+), #\\+[0-9]+\\]");
  std::regex mask_re =
      std::regex(start + "and r([0-9]+), r([0-9]+), r([0-9]+)");
  std::regex untag_re = std::regex(start + "mov r([0-9]+), r([0-9]+), asr #1");

  std::string poison_reg = "9";
};

TEST(DisasmPoisonMonomorphicLoad) {
#ifdef ENABLE_DISASSEMBLER
  if (i::FLAG_always_opt || !i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;
  i::FLAG_untrusted_code_mitigations = true;

  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function mono(o) { return o.x; };"
      "mono({ x : 1 });"
      "mono({ x : 1 });"
      "%OptimizeFunctionOnNextCall(mono);"
      "mono({ x : 1 });");

  Matchers m;

  std::smatch match;
  std::string line;
  std::istringstream reader(DisassembleFunction("mono"));
  bool poisoning_sequence_found = false;
  while (std::getline(reader, line)) {
    if (std::regex_match(line, match, m.map_load_re)) {
      std::string map_reg = match[1];
      std::string object_reg = match[2];
      // Matches that the property access sequence is instrumented with
      // poisoning. We match the following sequence:
      //
      // ldr r1, [r0, #-1]                ; load map
      // ldr r2, [pc, #+104]              ; load expected map constant
      // cmp r1, r2                       ; compare maps
      // bne ...                          ; deopt if different
      // eorne r9, r9, r9                 ; update the poison
      // csdb                             ; speculation barrier
      // ldr r0, [r0, #+11]               ; load the field
      // and r0, r0, r9                   ; apply the poison

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.load_const_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.cmp_re));
      CHECK_EQ(match[1], map_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.bne_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.eorne_re));
      CHECK_EQ(match[1], m.poison_reg);
      CHECK_EQ(match[2], m.poison_reg);
      CHECK_EQ(match[3], m.poison_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.csdb_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.load_field_re));
      CHECK_EQ(match[2], object_reg);
      std::string field_reg = match[1];

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.mask_re));
      CHECK_EQ(match[1], field_reg);
      CHECK_EQ(match[2], field_reg);
      CHECK_EQ(match[3], m.poison_reg);

      poisoning_sequence_found = true;
      break;
    }
  }

  CHECK(poisoning_sequence_found);
#endif  // ENABLE_DISASSEMBLER
}

TEST(DisasmPoisonPolymorphicLoad) {
#ifdef ENABLE_DISASSEMBLER
  if (i::FLAG_always_opt || !i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;
  i::FLAG_untrusted_code_mitigations = true;

  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function poly(o) { return o.x + 1; };"
      "let o1 = { x : 1 };"
      "let o2 = { y : 1 };"
      "o2.x = 2;"
      "poly(o1);"
      "poly(o2);"
      "poly(o1);"
      "poly(o2);"
      "%OptimizeFunctionOnNextCall(poly);"
      "poly(o1);");

  Matchers m;

  std::smatch match;
  std::string line;
  std::istringstream reader(DisassembleFunction("poly"));
  bool poisoning_sequence_found = false;
  while (std::getline(reader, line)) {
    if (std::regex_match(line, match, m.map_load_re)) {
      std::string map_reg = match[1];
      std::string object_reg = match[2];
      // Matches that the property access sequence is instrumented with
      // poisoning. We match the following sequence:
      //
      //   ldr r1, [r0, #-1]                ; load map
      //   ldr r2, [pc, #+104]              ; load map constant #1
      //   cmp r1, r2                       ; compare maps
      //   beq +Lcase1                      ; if match, got to the load
      //   eoreq r9, r9, r9                 ; update the poison
      //   csdb                             ; speculation barrier
      //   ldr r1, [r0, #-1]                ; load map
      //   ldr r2, [pc, #+304]              ; load map constant #2
      //   cmp r1, r2                       ; compare maps
      //   bne +Ldeopt                      ; deopt if different
      //   eorne r9, r9, r9                 ; update the poison
      //   csdb                             ; speculation barrier
      //   ldr r0, [r0, #+11]               ; load the field
      //   and r0, r0, r9                   ; apply the poison
      //   mov r0, r0, asr #1               ; untag
      //   b +Ldone                         ; goto merge point
      // Lcase1:
      //   eorne r9, r9, r9                 ; update the poison
      //   csdb                             ; speculation barrier
      //   ldr r0, [r0, #+3]                ; load property backing store
      //   and r0, r0, r9                   ; apply the poison
      //   ldr r0, [r0, #+3]                ; load the property
      //   and r0, r0, r9                   ; apply the poison
      // Ldone:

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.load_const_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.cmp_re));
      CHECK_EQ(match[1], map_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.beq_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.eoreq_re));
      CHECK_EQ(match[1], m.poison_reg);
      CHECK_EQ(match[2], m.poison_reg);
      CHECK_EQ(match[3], m.poison_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.csdb_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.map_load_re));
      map_reg = match[1];
      CHECK_EQ(match[2], object_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.load_const_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.cmp_re));
      CHECK_EQ(match[1], map_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.bne_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.eorne_re));
      CHECK_EQ(match[1], m.poison_reg);
      CHECK_EQ(match[2], m.poison_reg);
      CHECK_EQ(match[3], m.poison_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.csdb_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.load_field_re));
      CHECK_EQ(match[2], object_reg);
      std::string field_reg = match[1];

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.mask_re));
      CHECK_EQ(match[1], field_reg);
      CHECK_EQ(match[2], field_reg);
      CHECK_EQ(match[3], m.poison_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.untag_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.b_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.eorne_re));
      CHECK_EQ(match[1], m.poison_reg);
      CHECK_EQ(match[2], m.poison_reg);
      CHECK_EQ(match[3], m.poison_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.csdb_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.load_field_re));
      CHECK_EQ(match[2], object_reg);
      std::string storage_reg = match[1];

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.mask_re));
      CHECK_EQ(match[1], storage_reg);
      CHECK_EQ(match[2], storage_reg);
      CHECK_EQ(match[3], m.poison_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.load_field_re));
      CHECK_EQ(match[2], storage_reg);
      field_reg = match[1];

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, m.mask_re));
      CHECK_EQ(match[1], field_reg);
      CHECK_EQ(match[2], field_reg);
      CHECK_EQ(match[3], m.poison_reg);

      poisoning_sequence_found = true;
      break;
    }
  }

  CHECK(poisoning_sequence_found);
#endif  // ENABLE_DISASSEMBLER
}

}  // namespace internal
}  // namespace v8
