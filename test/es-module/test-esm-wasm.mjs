import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { strictEqual, match, ok, notStrictEqual } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: WASM modules', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should load exports', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual } from "node:assert";',
        `import * as wasmExports from ${JSON.stringify(fixtures.fileURL('es-modules/export-name-syntax-error.wasm'))};`,
        'assert.strictEqual(wasmExports["?f!o:o<b>a[r]"]?.value, 12682);',
      ].join('\n'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
  });

  it('should properly escape import names as well', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-wasm-modules',
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

  it('should emit experimental warning', async () => {
    const { code, signal, stderr } = await spawnPromisified(execPath, [
      '--experimental-wasm-modules',
      fixtures.path('es-modules/wasm-modules.mjs'),
    ]);

    strictEqual(code, 0);
    strictEqual(signal, null);
    match(stderr, /ExperimentalWarning/);
    match(stderr, /WebAssembly/);
  });

  it('should support top-level execution', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
      '--experimental-wasm-modules',
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
});
