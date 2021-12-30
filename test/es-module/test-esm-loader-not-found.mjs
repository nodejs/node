import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--experimental-loader',
  'i-dont-exist',
  path('/print-error-message.js'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', () => {
  stderr = stderr.toString();
  ok(stderr.includes(
    'Error [ERR_MODULE_NOT_FOUND]: Cannot find package \'i-dont-exist\' ' +
    'imported from'
  ));
  ok(!stderr.includes('Bad command or file name'));
});
