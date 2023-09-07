// Flags: --experimental-wasm-modules
import { mustCall } from '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { strictEqual } from 'assert';
import { spawn } from 'child_process';

{
  const entry = path('/es-modules/package-type-module/noext-wasm');

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
