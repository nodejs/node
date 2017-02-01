// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/wasm-section-fuzzers.h"

#include "include/v8.h"
#include "src/isolate.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

using namespace v8::internal::wasm;

static const char* kNameString = "name";
static const size_t kNameStringLength = 4;

int fuzz_wasm_section(WasmSectionCode section, const uint8_t* data,
                      size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  v8::internal::Isolate* i_isolate =
      reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);

  v8::internal::AccountingAllocator allocator;
  v8::internal::Zone zone(&allocator);

  ZoneBuffer buffer(&zone);
  buffer.write_u32(kWasmMagic);
  buffer.write_u32(kWasmVersion);
  if (section == kNameSectionCode) {
    buffer.write_u8(kUnknownSectionCode);
    buffer.write_size(size + kNameStringLength + 1);
    buffer.write_u8(kNameStringLength);
    buffer.write(reinterpret_cast<const uint8_t*>(kNameString),
                 kNameStringLength);
    buffer.write(data, size);
  } else {
    buffer.write_u8(section);
    buffer.write_size(size);
    buffer.write(data, size);
  }

  ErrorThrower thrower(i_isolate, "decoder");

  std::unique_ptr<const WasmModule> module(testing::DecodeWasmModuleForTesting(
      i_isolate, &zone, &thrower, buffer.begin(), buffer.end(), kWasmOrigin));

  return 0;
}
