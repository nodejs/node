import '../common/index.mjs';
import { exec } from 'child_process';
import { ok, strictEqual } from 'assert';

const expectedMessage = [
  'Command failed: node --es-module-specifier-resolution=node',
  ' --experimental-specifier-resolution=node\n',
  'node: bad option: cannot use --es-module-specifier-resolution',
  'and --experimental-specifier-resolution at the same time'
].join(' ');

function handleError(error, stdout, stderr) {
  ok(error);
  strictEqual(error.message, expectedMessage);
}

const flags = [
  '--es-module-specifier-resolution=node',
  '--experimental-specifier-resolution=node'
].join(' ');

exec(`${process.execPath} ${flags}`, {
  timeout: 300
}, handleError);
