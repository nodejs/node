import '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--experimental-loader',
  fileURL('es-module-loaders', 'syntax-error.mjs').href,
  fileURL('print-error-message.js').href,
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', () => {
  ok(stderr.includes('SyntaxError:') || console.error(stderr));
  ok(!stderr.includes('Bad command or file name') || console.error(stderr));
});
