import { mustCall } from '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { match, ok, notStrictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--experimental-loader',
  'i-dont-exist',
  path('print-error-message.js'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', mustCall((code, _signal) => {
  notStrictEqual(code, 0);

  // Error [ERR_MODULE_NOT_FOUND]: Cannot find package 'i-dont-exist'
  // imported from
  match(stderr, /ERR_MODULE_NOT_FOUND/);
  match(stderr, /'i-dont-exist'/);

  ok(!stderr.includes('Bad command or file name'));
}));
