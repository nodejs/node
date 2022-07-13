import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { strictEqual, match } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

import spawn from './helper.spawnAsPromised.mjs';


describe('ESM: WASM modules', { concurrency: true }, () => {
  it('should load exports', async () => {
    const { code, stderr, stdout } = await spawn(execPath, [
      '--no-warnings',
      '--experimental-wasm-modules',
      '--input-type=module',
      '--eval',
      [
        'import { strictEqual, match } from "node:assert";',
        `import { add, addImported } from "${path('es-modules/simple.wasm')}";`,
        `import { state } from "${path('es-modules/wasm-dep.mjs')}";`,
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

  it('should emit experimental warning', async () => {
    const { code, signal, stderr } = await spawn(execPath, [
      '--experimental-wasm-modules',
      path('/es-modules/wasm-modules.mjs'),
    ]);

    strictEqual(code, 0);
    strictEqual(signal, null);
    match(stderr, /ExperimentalWarning/);
    match(stderr, /WebAssembly/);
  });
});
