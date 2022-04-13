import { mustCall } from '../common/index.mjs';
import { fileURL, path } from '../common/fixtures.mjs';
import { match, ok, notStrictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--experimental-loader',
  fileURL('es-module-loaders', 'syntax-error.mjs').href,
  path('print-error-message.js'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', mustCall((code, _signal) => {
  notStrictEqual(code, 0);

  match(stderr, /SyntaxError:/);

  ok(!stderr.includes('Bad command or file name'));
}));
