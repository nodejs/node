import { mustCall } from '../common/index.mjs';
import { match, notStrictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

{
  const child = spawn(execPath, [
    '--experimental-network-imports',
    '--input-type=module',
    '-e',
    'import "http://example.com"',
  ]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', mustCall((code, _signal) => {
    notStrictEqual(code, 0);

    // [ERR_NETWORK_IMPORT_DISALLOWED]: import of 'http://example.com/' by
    //   …/[eval1] is not supported: http can only be used to load local
    // resources (use https instead).
    match(stderr, /[ERR_NETWORK_IMPORT_DISALLOWED]/);
  }));
}
{
  const child = spawn(execPath, [
    '--experimental-network-imports',
    '--input-type=module',
  ]);
  child.stdin.end('import "http://example.com"');

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', mustCall((code, _signal) => {
    notStrictEqual(code, 0);

    // [ERR_NETWORK_IMPORT_DISALLOWED]: import of 'http://example.com/' by
    //   …/[stdin] is not supported: http can only be used to load local
    // resources (use https instead).
    match(stderr, /[ERR_NETWORK_IMPORT_DISALLOWED]/);
  }));
}
