import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { ok } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const child = spawn(execPath, [
  '--experimental-json-modules',
  path('/es-modules/import-json-named-export.mjs'),
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', () => {
  // SyntaxError: The requested module '../experimental.json'
  // does not provide an export named 'ofLife'
  ok(stderr.includes('SyntaxError:'));
  ok(stderr.includes('\'../experimental.json\''));
  ok(stderr.includes('\'ofLife\''));
});
