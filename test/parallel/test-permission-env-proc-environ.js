'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (!common.isLinux) {
  common.skip('/proc/<pid>/environ is specific to Linux');
}

const assert = require('assert');
const fs = require('fs');
const { spawn } = require('child_process');
const { spawnSyncAndAssert } = require('../common/child_process');

const env = {
  ...process.env,
  PERMISSION_ENV_SECRET: 'secret-value',
  PERMISSION_ENV_ALLOWED: 'allowed-value',
};

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// A symbolic link that reaches the environ file of this process, which is
// the parent of the processes below, under another name.
const parentEnvironLink = tmpdir.resolve('parent-environ');
fs.symlinkSync(`/proc/${process.pid}/environ`, parentEnvironLink);

// The source of an array of the paths that reach the environ file of the
// parent process, evaluated in the child.
const parentPaths = `[
  '/proc/' + process.ppid + '/environ',
  '/proc/' + process.ppid + '/task/' + process.ppid + '/environ',
  // Symbolic links resolve differently than the paths lexically do.
  '/dev/fd/../../' + process.ppid + '/environ',
  '/proc/self/root/proc/' + process.ppid + '/environ',
  ${JSON.stringify(parentEnvironLink)},
]`;

// The same for the environ file of the process itself.
const ownPaths = `[
  '/proc/self/environ',
  '/proc/thread-self/environ',
  '/proc/' + process.pid + '/environ',
  '/proc/self/task/' + process.pid + '/environ',
  '/dev/fd/../environ',
]`;

// Source that asserts, in the child, that reading each of `paths` is denied.
const assertDenied = (paths) => `
  for (const path of ${paths}) {
    require('assert').throws(() => require('fs').readFileSync(path), {
      code: 'ERR_ACCESS_DENIED',
      permission: 'FileSystemRead',
      resource: path,
    }, path);
  }
`;

// /proc/<pid>/environ exposes the environment a process started with, so
// reading it is denied while the env scope is restricted, even when the file
// system scope would allow it.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-fs-read=*', '-e',
    assertDenied(ownPaths) + assertDenied(parentPaths),
  ],
  { env },
  {});

// Relative paths are resolved against the working directory.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-fs-read=*', '--allow-env=*', '-e',
    assertDenied(`[
      process.ppid + '/environ',
      '/proc/self/cwd/' + process.ppid + '/environ',
    ]`),
  ],
  { env, cwd: '/proc' },
  {});

// Reading its own is allowed when every variable is accessible, but the
// environment of any other process stays out of reach: a child process is
// started with --allow-env=*, and must not see what its parent could not.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-fs-read=*', '--allow-env=*', '-e',
    `
    for (const path of ${ownPaths}) require('fs').readFileSync(path);
    ${assertDenied(parentPaths)}
    `,
  ],
  { env },
  {});

// The same holds for a child process that a restricted process spawns, which
// inherits --allow-env=*.
{
  const grandchild = `
    require('fs').readFileSync('/proc/self/environ');
    ${assertDenied(`[
      '/proc/' + process.ppid + '/environ',
      '/dev/fd/../../' + process.ppid + '/environ',
    ]`)}
  `;
  spawnSyncAndAssert(
    process.execPath,
    [
      '--permission', '--allow-fs-read=*', '--allow-child-process', '-e',
      `require('child_process').execFileSync(
         process.execPath, ['-e', ${JSON.stringify(grandchild)}],
         { stdio: 'inherit' });`,
    ],
    { env },
    {});
}

// Other files on procfs are not affected.
spawnSyncAndAssert(
  process.execPath,
  [
    '--permission', '--allow-fs-read=*', '-e',
    `
    const fs = require('fs');
    fs.readFileSync('/proc/self/stat');
    fs.readFileSync('/proc/' + process.ppid + '/stat');
    fs.readdirSync('/proc/self');
    `,
  ],
  { env },
  {});

// Other processes do not find the removed variables there either, because
// they are overwritten in the initial environment block.
{
  const child = spawn(
    process.execPath,
    [
      '--permission', '--allow-env=PERMISSION_ENV_ALLOWED', '-e',
      'console.log("ready"); process.stdin.once("data", () => {});',
    ],
    { env, stdio: ['pipe', 'pipe', 'inherit'] });

  child.stdout.once('data', common.mustCall(() => {
    const environ = fs.readFileSync(`/proc/${child.pid}/environ`, 'latin1');
    assert.doesNotMatch(environ, /PERMISSION_ENV_SECRET|secret-value/);
    assert.match(environ, /PERMISSION_ENV_ALLOWED=allowed-value/);
    child.stdin.end('done');
  }));

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}

// permission.drop() overwrites the initial environment block too, so a
// variable that startup kept does not stay readable there after it is
// dropped at runtime.
{
  const child = spawn(
    process.execPath,
    [
      '--permission', '--allow-env=PERMISSION_ENV_*', '-e',
      `
      console.log('ready');
      process.stdin.once('data', () => {
        process.permission.drop('env', 'PERMISSION_ENV_SECRET');
        console.log('dropped');
        process.stdin.once('data', () => {});
      });
      `,
    ],
    { env, stdio: ['pipe', 'pipe', 'inherit'] });

  const readEnviron = () =>
    fs.readFileSync(`/proc/${child.pid}/environ`, 'latin1');

  child.stdout.once('data', common.mustCall(() => {
    // Both are allowed at startup, so both are still in the initial block.
    const environ = readEnviron();
    assert.match(environ, /PERMISSION_ENV_SECRET=secret-value/);
    assert.match(environ, /PERMISSION_ENV_ALLOWED=allowed-value/);

    child.stdout.once('data', common.mustCall(() => {
      const scrubbed = readEnviron();
      assert.doesNotMatch(scrubbed, /PERMISSION_ENV_SECRET|secret-value/);
      // Variables that were not dropped are left alone.
      assert.match(scrubbed, /PERMISSION_ENV_ALLOWED=allowed-value/);
      child.stdin.end('done');
    }));

    child.stdin.write('drop');
  }));

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}

// Dropping the whole env scope does the same for every variable it removes.
{
  const child = spawn(
    process.execPath,
    [
      '--permission', '--allow-env=PERMISSION_ENV_*', '-e',
      `
      console.log('ready');
      process.stdin.once('data', () => {
        process.permission.drop('env');
        console.log('dropped');
        process.stdin.once('data', () => {});
      });
      `,
    ],
    { env, stdio: ['pipe', 'pipe', 'inherit'] });

  child.stdout.once('data', common.mustCall(() => {
    child.stdout.once('data', common.mustCall(() => {
      const environ = fs.readFileSync(`/proc/${child.pid}/environ`, 'latin1');
      assert.doesNotMatch(environ, /PERMISSION_ENV_SECRET|secret-value/);
      assert.doesNotMatch(environ, /PERMISSION_ENV_ALLOWED|allowed-value/);
      child.stdin.end('done');
    }));

    child.stdin.write('drop');
  }));

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}
