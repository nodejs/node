import { mustCall } from '../common/index.mjs';
import { fileURL, path } from '../common/fixtures.mjs';
import { match, ok, notStrictEqual, strictEqual } from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

{
  const child = spawn(execPath, [
    '--experimental-loader',
    fileURL('es-module-loaders', 'thenable-load-hook.mjs').href,
    path('es-modules', 'test-esm-ok.mjs'),
  ]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', mustCall((code, _signal) => {
    strictEqual(code, 0);
    ok(!stderr.includes('must not call'));
  }));
}

{
  const child = spawn(execPath, [
    '--experimental-loader',
    fileURL('es-module-loaders', 'thenable-load-hook-rejected.mjs').href,
    path('es-modules', 'test-esm-ok.mjs'),
  ]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', mustCall((code, _signal) => {
    notStrictEqual(code, 0);

    match(stderr, /\sError: must crash the process\r?\n/);

    ok(!stderr.includes('must not call'));
  }));
}

{
  const child = spawn(execPath, [
    '--experimental-loader',
    fileURL('es-module-loaders', 'thenable-load-hook-rejected-no-arguments.mjs').href,
    path('es-modules', 'test-esm-ok.mjs'),
  ]);

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', mustCall((code, _signal) => {
    notStrictEqual(code, 0);

    match(stderr, /\sundefined\r?\n/);

    ok(!stderr.includes('must not call'));
  }));
}
