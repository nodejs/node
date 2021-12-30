import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

[
  path('/es-module-loaders/syntax-error-import.mjs'),
  path('/es-module-loaders/syntax-error-import-multiline.mjs'),
].forEach((entry) => {
  const child = spawn(execPath, [entry]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', () => {
    ok(stderr.toString().includes(
      'SyntaxError: The requested module \'./module-named-exports.mjs\' ' +
      'does not provide an export named \'notfound\''
    ));
  });
});
