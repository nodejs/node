// Test fixture for all WebAssembly global types
import { strictEqual, ok, throws } from 'node:assert';

// WASM SIMD is not supported on older architectures such as IBM Power8
let wasmExports;
try {
  wasmExports = await import('./globals.wasm');
} catch (e) {
  ok(e instanceof WebAssembly.CompileError);
  ok(e.message.includes('SIMD unsupported'));
}

if (wasmExports) {
  // Test imported globals using direct access
  strictEqual(wasmExports.importedI32, 42);
  strictEqual(wasmExports.importedMutI32, 100);
  strictEqual(wasmExports.importedI64, 9223372036854775807n);
  strictEqual(wasmExports.importedMutI64, 200n);
  strictEqual(Math.round(wasmExports.importedF32 * 100000) / 100000, 3.14159);
  strictEqual(Math.round(wasmExports.importedMutF32 * 100000) / 100000, 2.71828);
  strictEqual(wasmExports.importedF64, 3.141592653589793);
  strictEqual(wasmExports.importedMutF64, 2.718281828459045);
  strictEqual(wasmExports.importedExternref !== null, true);
  strictEqual(wasmExports.importedMutExternref !== null, true);
  strictEqual(wasmExports.importedNullExternref, null);

  // Test local globals exported directly
  strictEqual(wasmExports['🚀localI32'], 42);
  strictEqual(wasmExports.localMutI32, 100);
  strictEqual(wasmExports.localI64, 9223372036854775807n);
  strictEqual(wasmExports.localMutI64, 200n);
  strictEqual(Math.round(wasmExports.localF32 * 100000) / 100000, 3.14159);
  strictEqual(Math.round(wasmExports.localMutF32 * 100000) / 100000, 2.71828);
  strictEqual(wasmExports.localF64, 2.718281828459045);
  strictEqual(wasmExports.localMutF64, 3.141592653589793);

  // Test imported globals using getter functions
  strictEqual(wasmExports.getImportedMutI32(), 100);
  strictEqual(wasmExports.getImportedMutI64(), 200n);
  strictEqual(Math.round(wasmExports.getImportedMutF32() * 100000) / 100000, 2.71828);
  strictEqual(wasmExports.getImportedMutF64(), 2.718281828459045);
  strictEqual(wasmExports.getImportedMutExternref() !== null, true);

  // Test local globals using getter functions
  strictEqual(wasmExports.getLocalMutI32(), 100);
  strictEqual(wasmExports.getLocalMutI64(), 200n);
  strictEqual(Math.round(wasmExports.getLocalMutF32() * 100000) / 100000, 2.71828);
  strictEqual(wasmExports.getLocalMutF64(), 3.141592653589793);
  strictEqual(wasmExports.getLocalMutExternref(), null);

  throws(wasmExports.getLocalMutV128);

  // Pending TDZ support
  strictEqual(wasmExports.depV128, undefined);

  // Test modifying mutable globals and reading the new values
  wasmExports.setImportedMutI32(999);
  strictEqual(wasmExports.getImportedMutI32(), 999);

  wasmExports.setImportedMutI64(888n);
  strictEqual(wasmExports.getImportedMutI64(), 888n);

  wasmExports.setImportedMutF32(7.77);
  strictEqual(Math.round(wasmExports.getImportedMutF32() * 100) / 100, 7.77);

  wasmExports.setImportedMutF64(6.66);
  strictEqual(wasmExports.getImportedMutF64(), 6.66);

  // Test modifying mutable externref
  const testObj = { test: 'object' };
  wasmExports.setImportedMutExternref(testObj);
  strictEqual(wasmExports.getImportedMutExternref(), testObj);

  // Test modifying local mutable globals
  wasmExports.setLocalMutI32(555);
  strictEqual(wasmExports.getLocalMutI32(), 555);

  wasmExports.setLocalMutI64(444n);
  strictEqual(wasmExports.getLocalMutI64(), 444n);

  wasmExports.setLocalMutF32(3.33);
  strictEqual(Math.round(wasmExports.getLocalMutF32() * 100) / 100, 3.33);

  wasmExports.setLocalMutF64(2.22);
  strictEqual(wasmExports.getLocalMutF64(), 2.22);

  // These mutating pending live bindings support
  strictEqual(wasmExports.localMutI32, 100);
  strictEqual(wasmExports.localMutI64, 200n);
  strictEqual(Math.round(wasmExports.localMutF32 * 100) / 100, 2.72);
  strictEqual(wasmExports.localMutF64, 3.141592653589793);

  // Test modifying local mutable externref
  const anotherTestObj = { another: 'test object' };
  wasmExports.setLocalMutExternref(anotherTestObj);
  strictEqual(wasmExports.getLocalMutExternref(), anotherTestObj);
  strictEqual(wasmExports.localMutExternref, null);

  // Test dep.wasm imports
  strictEqual(wasmExports.depI32, 1001);
  strictEqual(wasmExports.depMutI32, 2001);
  strictEqual(wasmExports.getDepMutI32(), 2001);
  strictEqual(wasmExports.depI64, 10000000001n);
  strictEqual(wasmExports.depMutI64, 20000000001n);
  strictEqual(wasmExports.getDepMutI64(), 20000000001n);
  strictEqual(Math.round(wasmExports.depF32 * 100) / 100, 10.01);
  strictEqual(Math.round(wasmExports.depMutF32 * 100) / 100, 20.01);
  strictEqual(Math.round(wasmExports.getDepMutF32() * 100) / 100, 20.01);
  strictEqual(wasmExports.depF64, 100.0001);
  strictEqual(wasmExports.depMutF64, 200.0001);
  strictEqual(wasmExports.getDepMutF64(), 200.0001);

  // Test modifying dep.wasm mutable globals
  wasmExports.setDepMutI32(3001);
  strictEqual(wasmExports.getDepMutI32(), 3001);

  wasmExports.setDepMutI64(30000000001n);
  strictEqual(wasmExports.getDepMutI64(), 30000000001n);

  wasmExports.setDepMutF32(30.01);
  strictEqual(Math.round(wasmExports.getDepMutF32() * 100) / 100, 30.01);

  wasmExports.setDepMutF64(300.0001);
  strictEqual(wasmExports.getDepMutF64(), 300.0001);

  // These pending live bindings support
  strictEqual(wasmExports.depMutI32, 2001);
  strictEqual(wasmExports.depMutI64, 20000000001n);
  strictEqual(Math.round(wasmExports.depMutF32 * 100) / 100, 20.01);
  strictEqual(wasmExports.depMutF64, 200.0001);
}
