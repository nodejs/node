// META: global=window,dedicatedworker,jsshell,shadowrealm

"use strict";

promise_test(async () => {
  const mod = await import("./resources/exports.wasm");

  assert_array_equals(Object.getOwnPropertyNames(mod).sort(), [
    "a\u200Bb\u0300c",
    "func",
    "glob",
    "mem",
    "tab",
    "value with spaces",
    "🎯test-func!",
  ]);
  assert_true(mod.func instanceof Function);
  assert_true(mod.mem instanceof WebAssembly.Memory);
  assert_true(mod.tab instanceof WebAssembly.Table);

  assert_false(mod.glob instanceof WebAssembly.Global);
  assert_equals(typeof mod.glob, "number");

  assert_throws_js(TypeError, () => {
    mod.func = 2;
  });

  assert_equals(typeof mod["value with spaces"], "number");
  assert_equals(mod["value with spaces"], 123);

  assert_true(mod["🎯test-func!"] instanceof Function);
  assert_equals(mod["🎯test-func!"](), 456);

  assert_equals(typeof mod["a\u200Bb\u0300c"], "number");
  assert_equals(mod["a\u200Bb\u0300c"], 789);
}, "Exported names from a WebAssembly module");
