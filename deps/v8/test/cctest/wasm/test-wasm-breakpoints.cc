// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-interface.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-objects.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

using namespace v8::internal;
using namespace v8::internal::wasm;
namespace debug = v8::debug;

namespace {

void CheckLocations(
    WasmCompiledModule *compiled_module, debug::Location start,
    debug::Location end,
    std::initializer_list<debug::Location> expected_locations_init) {
  std::vector<debug::Location> locations;
  bool success =
      compiled_module->GetPossibleBreakpoints(start, end, &locations);
  CHECK(success);

  printf("got %d locations: ", static_cast<int>(locations.size()));
  for (size_t i = 0, e = locations.size(); i != e; ++i) {
    printf("%s<%d,%d>", i == 0 ? "" : ", ", locations[i].GetLineNumber(),
           locations[i].GetColumnNumber());
  }
  printf("\n");

  std::vector<debug::Location> expected_locations(expected_locations_init);
  CHECK_EQ(expected_locations.size(), locations.size());
  for (size_t i = 0, e = locations.size(); i != e; ++i) {
    CHECK_EQ(expected_locations[i].GetLineNumber(),
             locations[i].GetLineNumber());
    CHECK_EQ(expected_locations[i].GetColumnNumber(),
             locations[i].GetColumnNumber());
  }
}
void CheckLocationsFail(WasmCompiledModule *compiled_module,
                        debug::Location start, debug::Location end) {
  std::vector<debug::Location> locations;
  bool success =
      compiled_module->GetPossibleBreakpoints(start, end, &locations);
  CHECK(!success);
}

}  // namespace

TEST(CollectPossibleBreakpoints) {
  WasmRunner<int> runner(kExecuteCompiled);

  BUILD(runner, WASM_NOP, WASM_I32_ADD(WASM_ZERO, WASM_ONE));

  Handle<WasmInstanceObject> instance = runner.module().instance_object();
  std::vector<debug::Location> locations;
  CheckLocations(instance->compiled_module(), {0, 0}, {1, 0},
                 {{0, 1}, {0, 2}, {0, 4}, {0, 6}, {0, 7}});
  CheckLocations(instance->compiled_module(), {0, 2}, {0, 4}, {{0, 2}});
  CheckLocations(instance->compiled_module(), {0, 2}, {0, 5}, {{0, 2}, {0, 4}});
  CheckLocations(instance->compiled_module(), {0, 7}, {0, 8}, {{0, 7}});
  CheckLocations(instance->compiled_module(), {0, 7}, {1, 0}, {{0, 7}});
  CheckLocations(instance->compiled_module(), {0, 8}, {1, 0}, {});
  CheckLocationsFail(instance->compiled_module(), {0, 9}, {1, 0});
}
