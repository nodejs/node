import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--experimental-loader',
  path('/es-module-loaders/syntax-error.mjs'),
  path('/print-error-message.js'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', () => {
  stderr = stderr.toString()
  ok(stderr.includes(
    'SyntaxError: Malformed arrow function parameter list'
  ));
  ok(!stderr.includes('Bad command or file name'));
});
