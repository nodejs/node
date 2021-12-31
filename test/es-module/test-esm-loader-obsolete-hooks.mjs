import '../common/index.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--no-warnings',
  '--throw-deprecation',
  '--experimental-loader',
  './test/fixtures/es-module-loaders/hooks-obsolete.mjs',
  './test/fixtures/print-error-message.js',
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', () => {
  // DeprecationWarning: Obsolete loader hook(s) supplied and will be ignored:
  // dynamicInstantiate, getFormat, getSource, transformSource
  ok(stderr.includes('DeprecationWarning:') || console.error(stderr));
  ok(stderr.includes('dynamicInstantiate') || console.error(stderr));
  ok(stderr.includes('getFormat') || console.error(stderr));
  ok(stderr.includes('getSource') || console.error(stderr));
  ok(stderr.includes('transformSource') || console.error(stderr));
});
