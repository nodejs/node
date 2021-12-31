import '../common/index.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  './test/fixtures/es-module-loaders/syntax-error.mjs',
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', () => {
  ok(stderr.includes('SyntaxError:') || console.error(stderr));
});
