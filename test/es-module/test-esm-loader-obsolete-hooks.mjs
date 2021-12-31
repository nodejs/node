import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--no-warnings',
  '--throw-deprecation',
  '--experimental-loader',
  path('/es-module-loaders/hooks-obsolete.mjs'),
  path('/print-error-message.js'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', () => {
  ok(stderr.includes(
    'DeprecationWarning: Obsolete loader hook(s) supplied and will be ' +
    'ignored: dynamicInstantiate, getFormat, getSource, transformSource'
  ) || console.error(stderr));
});
