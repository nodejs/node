import '../common/index.mjs';
import assert from 'assert';
import child_process from 'child_process';
import fixtures from '../common/fixtures.js';

const commonArgs = [
  '--no-warnings',
  '--input-type=module',
  '--eval',
];

{
  // Unresolved TLA promise, --eval
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    [...commonArgs, 'await new Promise(() => {})'],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout, stderr], [13, '', '']);
}

{
  // Rejected TLA promise, --eval
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    [...commonArgs, 'await Promise.reject(new Error("Xyz"))'],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout], [1, '']);
  assert.match(stderr, /Error: Xyz/);
}

{
  // Unresolved TLA promise with explicit exit code, --eval
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    [
      ...commonArgs,
      'process.exitCode = 42;await new Promise(() => {})',
    ],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout, stderr], [42, '', '']);
}

{
  // Rejected TLA promise with explicit exit code, --eval
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    [
      ...commonArgs,
      'process.exitCode = 42;await Promise.reject(new Error("Xyz"))',
    ],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout], [1, '']);
  assert.match(stderr, /Error: Xyz/);
}

{
  // Unresolved TLA promise, module file
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    ['--no-warnings', fixtures.path('es-modules/tla/unresolved.mjs')],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout, stderr], [13, '', '']);
}

{
  // Rejected TLA promise, module file
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    ['--no-warnings', fixtures.path('es-modules/tla/rejected.mjs')],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout], [1, '']);
  assert.match(stderr, /Error: Xyz/);
}

{
  // Unresolved TLA promise, module file
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    ['--no-warnings', fixtures.path('es-modules/tla/unresolved-withexitcode.mjs')],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout, stderr], [42, '', '']);
}

{
  // Rejected TLA promise, module file
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    ['--no-warnings', fixtures.path('es-modules/tla/rejected-withexitcode.mjs')],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout], [1, '']);
  assert.match(stderr, /Error: Xyz/);
}

{
  // Calling process.exit() in .mjs should return status 0
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    ['--no-warnings', fixtures.path('es-modules/tla/process-exit.mjs')],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout, stderr], [0, '', '']);
}

{
  // Calling process.exit() in worker thread shouldn't influence main thread
  const { status, stdout, stderr } = child_process.spawnSync(
    process.execPath,
    ['--no-warnings', fixtures.path('es-modules/tla/unresolved-with-worker-process-exit.mjs')],
    { encoding: 'utf8' });
  assert.deepStrictEqual([status, stdout, stderr], [13, '', '']);
}
