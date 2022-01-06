import { mustCall } from '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { match, notStrictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const importStatement =
  'import { foo, notfound } from \'./module-named-exports.mjs\';';
const importStatementMultiline = `import {
  foo,
  notfound
} from './module-named-exports.mjs';
`;

[importStatement, importStatementMultiline].forEach((input) => {
  const child = spawn(execPath, [
    '--input-type=module',
    '--eval',
    input,
  ], {
    cwd: path('es-module-loaders'),
  });

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', mustCall((code, _signal) => {
    notStrictEqual(code, 0);

    // SyntaxError: The requested module './module-named-exports.mjs'
    // does not provide an export named 'notfound'
    match(stderr, /SyntaxError:/);
    // The quotes ensure that the path starts with ./ and not ../
    match(stderr, /'\.\/module-named-exports\.mjs'/);
    match(stderr, /notfound/);
  }));
});
