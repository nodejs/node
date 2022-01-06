import { mustCall } from '../common/index.mjs';
import { fileURL, path } from '../common/fixtures.mjs';
import { match, notStrictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--no-warnings',
  '--throw-deprecation',
  '--experimental-loader',
  fileURL('es-module-loaders', 'hooks-obsolete.mjs').href,
  path('print-error-message.js'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', mustCall((code, _signal) => {
  notStrictEqual(code, 0);

  // DeprecationWarning: Obsolete loader hook(s) supplied and will be ignored:
  // dynamicInstantiate, getFormat, getSource, transformSource
  match(stderr, /DeprecationWarning:/);
  match(stderr, /dynamicInstantiate/);
  match(stderr, /getFormat/);
  match(stderr, /getSource/);
  match(stderr, /transformSource/);
}));
