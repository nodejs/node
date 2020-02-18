import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { strictEqual, ok } from 'assert';
import { spawn } from 'child_process';

const child = spawn(process.execPath, [
  '--experimental-import-meta-resolve',
  path('/es-modules/import-resolve-exports.mjs')
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
    'ExperimentalWarning: The ESM module loader is experimental'
  ));
  ok(!stderr.toString().includes(
    'ExperimentalWarning: Conditional exports'
  ));
});
