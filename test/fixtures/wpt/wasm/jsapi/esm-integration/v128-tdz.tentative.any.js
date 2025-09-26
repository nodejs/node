// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const exporterModule = await import("./resources/mutable-global-export.wasm");
  const reexporterModule = await import(
    "./resources/mutable-global-reexport.wasm"
  );

  assert_throws_js(ReferenceError, () => exporterModule.v128Export);
  assert_throws_js(ReferenceError, () => reexporterModule.reexportedV128Export);
}, "v128 global exports should cause TDZ errors");
