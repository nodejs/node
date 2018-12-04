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

  std::string start("0x[0-9a-f]+ +[0-9a-f]+ +[0-9a-f]+ +");
  std::regex map_load_re(start + "ldr r([0-9]+), \\[r([0-9]+), #-1\\]");
  std::regex load_const_re(start + "ldr r([0-9]+), \\[pc, .*");
  std::regex cmp_re(start + "cmp r([0-9]+), r([0-9]+)");
  std::regex bne_re(start + "bne(.*)");
  std::regex eorne_re(start + "eorne r([0-9]+), r([0-9]+), r([0-9]+)");
  std::regex csdb_re(start + "csdb");
  std::regex load_field_re(start +
                           "ldr r([0-9]+), \\[r([0-9]+), #\\+[0-9]+\\]");
  std::regex mask_re(start + "and r([0-9]+), r([0-9]+), r([0-9]+)");

  std::string poison_reg = "9";

  std::smatch match;
  std::string line;
  std::istringstream reader(DisassembleFunction("mono"));
  bool poisoning_sequence_found = false;
  while (std::getline(reader, line)) {
    if (std::regex_match(line, match, map_load_re)) {
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
      CHECK(std::regex_match(line, match, load_const_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, cmp_re));
      CHECK_EQ(match[1], map_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, bne_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, eorne_re));
      CHECK_EQ(match[1], poison_reg);
      CHECK_EQ(match[2], poison_reg);
      CHECK_EQ(match[3], poison_reg);

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, csdb_re));

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, load_field_re));
      CHECK_EQ(match[2], object_reg);
      std::string field_reg = match[1];

      CHECK(std::getline(reader, line));
      CHECK(std::regex_match(line, match, mask_re));
      CHECK_EQ(match[1], field_reg);
      CHECK_EQ(match[2], field_reg);
      CHECK_EQ(match[3], poison_reg);

      poisoning_sequence_found = true;
      break;
    }
  }

  CHECK(poisoning_sequence_found);
#endif  // ENABLE_DISASSEMBLER
}

}  // namespace internal
}  // namespace v8
