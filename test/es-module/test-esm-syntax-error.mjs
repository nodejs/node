import { mustCall } from '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { match, notStrictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  path('es-module-loaders', 'syntax-error.mjs'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', mustCall((code, _signal) => {
  notStrictEqual(code, 0);
  match(stderr, /SyntaxError:/);
}));
