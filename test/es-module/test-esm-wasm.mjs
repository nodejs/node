// Flags: --experimental-wasm-modules
import { mustCall } from '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { add, addImported } from '../fixtures/es-modules/simple.wasm';
import { state } from '../fixtures/es-modules/wasm-dep.mjs';
import { strictEqual, ok } from 'assert';
import { spawn } from 'child_process';

strictEqual(state, 'WASM Start Executed');

strictEqual(add(10, 20), 30);

strictEqual(addImported(0), 42);

strictEqual(state, 'WASM JS Function Executed');

strictEqual(addImported(1), 43);

{
  // Test warning message
  const child = spawn(process.execPath, [
    '--experimental-wasm-modules',
    path('/es-modules/wasm-modules.mjs')
  ]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', (code, signal) => {
    strictEqual(code, 0);
    strictEqual(signal, null);
    ok(stderr.toString().includes(
      'ExperimentalWarning: Importing Web Assembly modules is ' +
      'an experimental feature. This feature could change at any time'
    ));
  });
}

{
  const entry = path('/es-modules/package-type-wasm/noext-wasm');

  // Run a module that does not have extension.
  // This is to ensure that "type": "module" applies to extensionless files.

  const child = spawn(process.execPath, ['--experimental-wasm-modules', entry]);

  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    stdout += data;
  });
  child.on('close', mustCall((code, signal) => {
    strictEqual(code, 0);
    strictEqual(signal, null);
    strictEqual(stdout, '');
  }));
}
