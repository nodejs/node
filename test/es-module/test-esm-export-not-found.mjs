import '../common/index.mjs';
import { fixturesDir } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { join } from 'path';
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
    cwd: join(fixturesDir, 'es-module-loaders')
  });

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', () => {
    // SyntaxError: The requested module './module-named-exports.mjs'
    // does not provide an export named 'notfound'
    ok(stderr.includes('SyntaxError:'));
    // The quotes ensure that the path starts with ./ and not ../
    ok(stderr.includes('\'./module-named-exports.mjs\''));
    ok(stderr.includes('notfound'));
  });
});
