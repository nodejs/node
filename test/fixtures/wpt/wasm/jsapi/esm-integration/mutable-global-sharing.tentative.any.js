promise_test(async () => {
  const exporterModule = await import("./resources/mutable-global-export.wasm");
  const reexporterModule = await import(
    "./resources/mutable-global-reexport.wasm"
  );

  assert_equals(exporterModule.mutableValue, 100);
  assert_equals(reexporterModule.reexportedMutableValue, 100);
}, "WebAssembly modules should export shared mutable globals with correct initial values");

promise_test(async () => {
  const exporterModule = await import("./resources/mutable-global-export.wasm");
  const reexporterModule = await import(
    "./resources/mutable-global-reexport.wasm"
  );

  exporterModule.setGlobal(500);

  assert_equals(exporterModule.getGlobal(), 500, "exporter should see 500");
  assert_equals(reexporterModule.getImportedGlobal(), 500);

  reexporterModule.setImportedGlobal(600);

  assert_equals(exporterModule.getGlobal(), 600);
  assert_equals(reexporterModule.getImportedGlobal(), 600);

  exporterModule.setGlobal(700);

  assert_equals(exporterModule.getGlobal(), 700);
  assert_equals(reexporterModule.getImportedGlobal(), 700);
}, "Wasm-to-Wasm mutable global sharing is live");

promise_test(async () => {
  const module1 = await import("./resources/mutable-global-export.wasm");
  const module2 = await import("./resources/mutable-global-export.wasm");

  assert_equals(module1, module2);

  module1.setGlobal(800);
  assert_equals(module1.getGlobal(), 800, "module1 should see its own change");
  assert_equals(module2.getGlobal(), 800);
}, "Multiple JavaScript imports return the same WebAssembly module instance");

promise_test(async () => {
  const exporterModule = await import("./resources/mutable-global-export.wasm");
  const reexporterModule = await import(
    "./resources/mutable-global-reexport.wasm"
  );

  assert_equals(exporterModule.getV128Lane(0), 1);
  assert_equals(exporterModule.getV128Lane(1), 2);
  assert_equals(exporterModule.getV128Lane(2), 3);
  assert_equals(exporterModule.getV128Lane(3), 4);

  assert_equals(reexporterModule.getImportedV128Lane(0), 1);
  assert_equals(reexporterModule.getImportedV128Lane(1), 2);
  assert_equals(reexporterModule.getImportedV128Lane(2), 3);
  assert_equals(reexporterModule.getImportedV128Lane(3), 4);
}, "v128 globals should work correctly in WebAssembly-to-WebAssembly imports");

promise_test(async () => {
  const exporterModule = await import("./resources/mutable-global-export.wasm");
  const reexporterModule = await import(
    "./resources/mutable-global-reexport.wasm"
  );

  exporterModule.setV128Global(10, 20, 30, 40);

  assert_equals(exporterModule.getV128Lane(0), 10);
  assert_equals(exporterModule.getV128Lane(1), 20);
  assert_equals(exporterModule.getV128Lane(2), 30);
  assert_equals(exporterModule.getV128Lane(3), 40);

  assert_equals(reexporterModule.getImportedV128Lane(0), 10);
  assert_equals(reexporterModule.getImportedV128Lane(1), 20);
  assert_equals(reexporterModule.getImportedV128Lane(2), 30);
  assert_equals(reexporterModule.getImportedV128Lane(3), 40);
}, "v128 global mutations should work correctly between WebAssembly modules");
