import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { strictEqual, match, ok, notStrictEqual } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: WASM modules', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should load exports', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual, match } from "node:assert";',
        `import { add, addImported } from ${JSON.stringify(fixtures.fileURL('es-modules/simple.wasm'))};`,
        `import { state } from ${JSON.stringify(fixtures.fileURL('es-modules/wasm-dep.mjs'))};`,
        'strictEqual(state, "WASM Start Executed");',
        'strictEqual(add(10, 20), 30);',
        'strictEqual(addImported(0), 42);',
        'strictEqual(state, "WASM JS Function Executed");',
        'strictEqual(addImported(1), 43);',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  it('should not allow code injection through export names', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `import * as wasmExports from ${JSON.stringify(fixtures.fileURL('es-modules/export-name-code-injection.wasm'))};`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  it('should allow non-identifier export names', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual } from "node:assert";',
        `import * as wasmExports from ${JSON.stringify(fixtures.fileURL('es-modules/export-name-syntax-error.wasm'))};`,
        'assert.strictEqual(wasmExports["?f!o:o<b>a[r]"], 12682);',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  it('should properly handle all WebAssembly global types', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual, deepStrictEqual, ok } from "node:assert";',

        // WASM SIMD is not supported on older architectures such as IBM Power8
        'let wasmExports;',
        'try {',
        `  wasmExports = await import(${JSON.stringify(fixtures.fileURL('es-modules/globals.wasm'))});`,
        '} catch (e) {',
        '  ok(e instanceof WebAssembly.CompileError);',
        '  ok(e.message.includes("SIMD unsupported"));',
        '}',

        'if (wasmExports) {',

        // Test imported globals using direct access
        '  strictEqual(wasmExports.importedI32, 42);',
        '  strictEqual(wasmExports.importedMutI32, 100);',
        '  strictEqual(wasmExports.importedI64, 9223372036854775807n);',
        '  strictEqual(wasmExports.importedMutI64, 200n);',
        '  strictEqual(Math.round(wasmExports.importedF32 * 100000) / 100000, 3.14159);',
        '  strictEqual(Math.round(wasmExports.importedMutF32 * 100000) / 100000, 2.71828);',
        '  strictEqual(wasmExports.importedF64, 3.141592653589793);',
        '  strictEqual(wasmExports.importedMutF64, 2.718281828459045);',
        '  strictEqual(wasmExports.importedExternref !== null, true);',
        '  strictEqual(wasmExports.importedMutExternref !== null, true);',
        '  strictEqual(wasmExports.importedNullExternref, null);',

        // Test local globals exported directly
        '  strictEqual(wasmExports[\'🚀localI32\'], 42);',
        '  strictEqual(wasmExports.localMutI32, 100);',
        '  strictEqual(wasmExports.localI64, 9223372036854775807n);',
        '  strictEqual(wasmExports.localMutI64, 200n);',
        '  strictEqual(Math.round(wasmExports.localF32 * 100000) / 100000, 3.14159);',
        '  strictEqual(Math.round(wasmExports.localMutF32 * 100000) / 100000, 2.71828);',
        '  strictEqual(wasmExports.localF64, 2.718281828459045);',
        '  strictEqual(wasmExports.localMutF64, 3.141592653589793);',

        // Test imported globals using getter functions
        '  strictEqual(wasmExports.getImportedMutI32(), 100);',
        '  strictEqual(wasmExports.getImportedMutI64(), 200n);',
        '  strictEqual(Math.round(wasmExports.getImportedMutF32() * 100000) / 100000, 2.71828);',
        '  strictEqual(wasmExports.getImportedMutF64(), 2.718281828459045);',
        '  strictEqual(wasmExports.getImportedMutExternref() !== null, true);',

        // Test local globals using getter functions
        '  strictEqual(wasmExports.getLocalMutI32(), 100);',
        '  strictEqual(wasmExports.getLocalMutI64(), 200n);',
        '  strictEqual(Math.round(wasmExports.getLocalMutF32() * 100000) / 100000, 2.71828);',
        '  strictEqual(wasmExports.getLocalMutF64(), 3.141592653589793);',
        '  strictEqual(wasmExports.getLocalMutExternref(), null);',

        '  assert.throws(wasmExports.getLocalMutV128);',

        // Pending TDZ support
        '  strictEqual(wasmExports.depV128, undefined);',

        // Test modifying mutable globals and reading the new values
        '  wasmExports.setImportedMutI32(999);',
        '  strictEqual(wasmExports.getImportedMutI32(), 999);',

        '  wasmExports.setImportedMutI64(888n);',
        '  strictEqual(wasmExports.getImportedMutI64(), 888n);',

        '  wasmExports.setImportedMutF32(7.77);',
        '  strictEqual(Math.round(wasmExports.getImportedMutF32() * 100) / 100, 7.77);',

        '  wasmExports.setImportedMutF64(6.66);',
        '  strictEqual(wasmExports.getImportedMutF64(), 6.66);',

        // Test modifying mutable externref
        '  const testObj = { test: "object" };',
        '  wasmExports.setImportedMutExternref(testObj);',
        '  strictEqual(wasmExports.getImportedMutExternref(), testObj);',

        // Test modifying local mutable globals
        '  wasmExports.setLocalMutI32(555);',
        '  strictEqual(wasmExports.getLocalMutI32(), 555);',

        '  wasmExports.setLocalMutI64(444n);',
        '  strictEqual(wasmExports.getLocalMutI64(), 444n);',

        '  wasmExports.setLocalMutF32(3.33);',
        '  strictEqual(Math.round(wasmExports.getLocalMutF32() * 100) / 100, 3.33);',

        '  wasmExports.setLocalMutF64(2.22);',
        '  strictEqual(wasmExports.getLocalMutF64(), 2.22);',

        // These mutating pending live bindings support
        '  strictEqual(wasmExports.localMutI32, 100);',
        '  strictEqual(wasmExports.localMutI64, 200n);',
        '  strictEqual(Math.round(wasmExports.localMutF32 * 100) / 100, 2.72);',
        '  strictEqual(wasmExports.localMutF64, 3.141592653589793);',

        // Test modifying local mutable externref
        '  const anotherTestObj = { another: "test object" };',
        '  wasmExports.setLocalMutExternref(anotherTestObj);',
        '  strictEqual(wasmExports.getLocalMutExternref(), anotherTestObj);',
        '  strictEqual(wasmExports.localMutExternref, null);',

        // Test dep.wasm imports
        '  strictEqual(wasmExports.depI32, 1001);',
        '  strictEqual(wasmExports.depMutI32, 2001);',
        '  strictEqual(wasmExports.getDepMutI32(), 2001);',
        '  strictEqual(wasmExports.depI64, 10000000001n);',
        '  strictEqual(wasmExports.depMutI64, 20000000001n);',
        '  strictEqual(wasmExports.getDepMutI64(), 20000000001n);',
        '  strictEqual(Math.round(wasmExports.depF32 * 100) / 100, 10.01);',
        '  strictEqual(Math.round(wasmExports.depMutF32 * 100) / 100, 20.01);',
        '  strictEqual(Math.round(wasmExports.getDepMutF32() * 100) / 100, 20.01);',
        '  strictEqual(wasmExports.depF64, 100.0001);',
        '  strictEqual(wasmExports.depMutF64, 200.0001);',
        '  strictEqual(wasmExports.getDepMutF64(), 200.0001);',

        // Test modifying dep.wasm mutable globals
        '  wasmExports.setDepMutI32(3001);',
        '  strictEqual(wasmExports.getDepMutI32(), 3001);',

        '  wasmExports.setDepMutI64(30000000001n);',
        '  strictEqual(wasmExports.getDepMutI64(), 30000000001n);',

        '  wasmExports.setDepMutF32(30.01);',
        '  strictEqual(Math.round(wasmExports.getDepMutF32() * 100) / 100, 30.01);',

        '  wasmExports.setDepMutF64(300.0001);',
        '  strictEqual(wasmExports.getDepMutF64(), 300.0001);',

        // These pending live bindings support
        '  strictEqual(wasmExports.depMutI32, 2001);',
        '  strictEqual(wasmExports.depMutI64, 20000000001n);',
        '  strictEqual(Math.round(wasmExports.depMutF32 * 100) / 100, 20.01);',
        '  strictEqual(wasmExports.depMutF64, 200.0001);',

        '}',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  it('should properly escape import names as well', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual } from "node:assert";',
        `import * as wasmExports from ${JSON.stringify(fixtures.fileURL('es-modules/import-name.wasm'))};`,
        'assert.strictEqual(wasmExports.xor(), 12345);',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  it('should emit experimental warning for module instances', async () => {
    const { code, signal, stderr } = await spawnPromisified(execPath, [
      fixtures.path('es-modules/wasm-modules.mjs'),
    ]);

    strictEqual(code, 0);
    strictEqual(signal, null);
    match(stderr, /ExperimentalWarning/);
    match(stderr, /Importing WebAssembly module instances/);
  });

  it('should support top-level execution', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-modules/top-level-wasm.wasm'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '[Object: null prototype] { prop: \'hello world\' }\n');
    strictEqual(code, 0);
  });

  // `import source` is not supported on this version of V8.
  it.skip('should support static source phase imports', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual } from "node:assert";',
        `import * as wasmExports from ${JSON.stringify(fixtures.fileURL('es-modules/wasm-source-phase.js'))};`,
        'strictEqual(wasmExports.mod instanceof WebAssembly.Module, true);',
        'const AbstractModuleSourceProto = Object.getPrototypeOf(Object.getPrototypeOf(wasmExports.mod));',
        'const toStringTag = Object.getOwnPropertyDescriptor(AbstractModuleSourceProto, Symbol.toStringTag).get;',
        'strictEqual(toStringTag.call(wasmExports.mod), "WebAssembly.Module");',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  // TODO: Enable this once https://github.com/nodejs/node/pull/56842 lands.
  it.skip('should support dynamic source phase imports', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual } from "node:assert";',
        `import * as wasmExports from ${JSON.stringify(fixtures.fileURL('es-modules/wasm-source-phase.js'))};`,
        'strictEqual(wasmExports.mod instanceof WebAssembly.Module, true);',
        'strictEqual(await wasmExports.dyn("./simple.wasm") instanceof WebAssembly.Module, true);',
        'const AbstractModuleSourceProto = Object.getPrototypeOf(Object.getPrototypeOf(wasmExports.mod));',
        'const toStringTag = Object.getOwnPropertyDescriptor(AbstractModuleSourceProto, Symbol.toStringTag).get;',
        'strictEqual(toStringTag.call(wasmExports.mod), "WebAssembly.Module");',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  // `import source` is not supported on this version of V8.
  it.skip('should not execute source phase imports', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual } from "node:assert";',
        `import source mod from ${JSON.stringify(fixtures.fileURL('es-modules/unimportable.wasm'))};`,
        'assert.strictEqual(mod instanceof WebAssembly.Module, true);',
        `await assert.rejects(import(${JSON.stringify(fixtures.fileURL('es-modules/unimportable.wasm'))}));`,
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  // TODO: Enable this once https://github.com/nodejs/node/pull/56842 lands.
  it.skip('should not execute dynamic source phase imports', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `await import.source(${JSON.stringify(fixtures.fileURL('es-modules/unimportable.wasm'))})`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  // TODO: Enable this once https://github.com/nodejs/node/pull/56842 lands.
  it.skip('should throw for dynamic source phase imports not defined', async () => {
    const fileUrl = fixtures.fileURL('es-modules/wasm-source-phase.js');
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { ok, strictEqual } from "node:assert";',
        `await assert.rejects(import.source(${JSON.stringify(fileUrl)}), (e) => {`,
        '  strictEqual(e instanceof SyntaxError, true);',
        '  strictEqual(e.message.includes("Source phase import object is not defined for module"), true);',
        `  strictEqual(e.message.includes(${JSON.stringify(fileUrl)}), true);`,
        '});',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  // `import source` is not supported on this version of V8.
  it.skip('should throw for static source phase imports not defined', async () => {
    const fileUrl = fixtures.fileURL('es-modules/wasm-source-phase.js');
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `import source nosource from ${JSON.stringify(fileUrl)};`,
    ]);
    match(stderr, /Source phase import object is not defined for module/);
    ok(stderr.includes(fileUrl));
    strictEqual(stdout, '');
    notStrictEqual(code, 0);
  });

  // `import source` is not supported on this version of V8
  it.skip('should throw for vm source phase static import', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-vm-modules',
      '--input-type=module',
      '--eval',
      [
        'const m1 = new vm.SourceTextModule("import source x from \\"y\\";");',
        'const m2 = new vm.SourceTextModule("export var p = 5;");',
        'await m1.link(() => m2);',
        'await m1.evaluate();',
      ].join('\n'),
    ]);
    match(stderr, /Source phase import object is not defined for module/);
    strictEqual(stdout, '');
    notStrictEqual(code, 0);
  });

  // TODO: Enable this once https://github.com/nodejs/node/pull/56842 lands.
  it.skip('should throw for vm source phase dynamic import', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-vm-modules',
      '--input-type=module',
      '--eval',
      [
        'import { constants } from "node:vm";',
        'const opts = { importModuleDynamically: () => m2 };',
        'const m1 = new vm.SourceTextModule("await import.source(\\"y\\");", opts);',
        'const m2 = new vm.SourceTextModule("export var p = 5;");',
        'await m1.link(() => m2);',
        'await m1.evaluate();',
      ].join('\n'),
    ]);
    match(stderr, /Source phase import object is not defined for module/);
    strictEqual(stdout, '');
    notStrictEqual(code, 0);
  });

  it('should reject wasm: import names', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `import(${JSON.stringify(fixtures.fileURL('es-modules/invalid-import-name.wasm'))})`,
    ]);

    match(stderr, /Invalid Wasm import name/);
    strictEqual(stdout, '');
    notStrictEqual(code, 0);
  });

  it('should reject wasm-js: import names', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `import(${JSON.stringify(fixtures.fileURL('es-modules/invalid-import-name-wasm-js.wasm'))})`,
    ]);

    match(stderr, /Invalid Wasm import name/);
    strictEqual(stdout, '');
    notStrictEqual(code, 0);
  });

  it('should reject wasm-js: import module names', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `import(${JSON.stringify(fixtures.fileURL('es-modules/invalid-import-module.wasm'))})`,
    ]);

    match(stderr, /Invalid Wasm import/);
    strictEqual(stdout, '');
    notStrictEqual(code, 0);
  });

  it('should reject wasm: export names', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `import(${JSON.stringify(fixtures.fileURL('es-modules/invalid-export-name.wasm'))})`,
    ]);

    match(stderr, /Invalid Wasm export/);
    strictEqual(stdout, '');
    notStrictEqual(code, 0);
  });

  it('should reject wasm-js: export names', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `import(${JSON.stringify(fixtures.fileURL('es-modules/invalid-export-name-wasm-js.wasm'))})`,
    ]);

    match(stderr, /Invalid Wasm export/);
    strictEqual(stdout, '');
    notStrictEqual(code, 0);
  });

  it('should support js-string builtins', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual } from "node:assert";',
        `import * as wasmExports from ${JSON.stringify(fixtures.fileURL('es-modules/js-string-builtins.wasm'))};`,
        'strictEqual(wasmExports.getLength("hello"), 5);',
        'strictEqual(wasmExports.concatStrings("hello", " world"), "hello world");',
        'strictEqual(wasmExports.compareStrings("test", "test"), 1);',
        'strictEqual(wasmExports.compareStrings("test", "different"), 0);',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });
});
