import { mustCall } from '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { match, notStrictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--experimental-json-modules',
  path('es-modules', 'import-json-named-export.mjs'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', mustCall((code, _signal) => {
  notStrictEqual(code, 0);

  // SyntaxError: The requested module '../experimental.json'
  // does not provide an export named 'ofLife'
  match(stderr, /SyntaxError:/);
  match(stderr, /'\.\.\/experimental\.json'/);
  match(stderr, /'ofLife'/);
}));
