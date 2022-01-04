import '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--experimental-loader',
  'i-dont-exist',
  fileURL('print-error-message.js').href,
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', () => {
  // Error [ERR_MODULE_NOT_FOUND]: Cannot find package 'i-dont-exist'
  // imported from
  ok(stderr.includes('ERR_MODULE_NOT_FOUND') || console.error(stderr));
  ok(stderr.includes('\'i-dont-exist\'') || console.error(stderr));

  ok(!stderr.includes('Bad command or file name') || console.error(stderr));
});
