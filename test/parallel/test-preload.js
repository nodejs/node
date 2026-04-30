'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
// Refs: https://github.com/nodejs/node/pull/2253
if (common.isSunOS)
  common.skip('unreliable on SunOS');

const assert = require('assert');
const childProcess = require('child_process');

const nodeBinary = process.argv[0];

const preloadOption = (...preloads) => {
  return preloads.flatMap((preload) => ['-r', preload]);
};

const fixtureA = fixtures.path('printA.js');
const fixtureB = fixtures.path('printB.js');
const fixtureC = fixtures.path('printC.js');
const fixtureD = fixtures.path('define-global.js');
const fixtureE = fixtures.path('intrinsic-mutation.js');
const fixtureF = fixtures.path('print-intrinsic-mutation-name.js');
const fixtureThrows = fixtures.path('throws_error4.js');
const fixtureIsPreloading = fixtures.path('ispreloading.js');

// Assert that module.isPreloading is false here
assert(!module.isPreloading);

// Test that module.isPreloading is set in preloaded module
// Test preloading a single module works
common.spawnPromisified(nodeBinary, [...preloadOption(fixtureIsPreloading), fixtureB]).then(common.mustCall(
  ({ stdout }) => {
    assert.strictEqual(stdout.trim(), 'B');
  }
));

// Test preloading a single module works
common.spawnPromisified(nodeBinary, [...preloadOption(fixtureA), fixtureB]).then(common.mustCall(
  ({ stdout }) => {
    assert.strictEqual(stdout, 'A\nB\n');
  }));

// Test preloading multiple modules works
common.spawnPromisified(nodeBinary, [...preloadOption(fixtureA, fixtureB), fixtureC]).then(common.mustCall(
  ({ stdout }) => {
    assert.strictEqual(stdout, 'A\nB\nC\n');
  }
));

// Test that preloading a throwing module aborts
common.spawnPromisified(nodeBinary, [...preloadOption(fixtureA, fixtureThrows), fixtureB]).then(common.mustCall(
  ({ code, stdout }) => {
    assert.strictEqual(stdout, 'A\n');
    assert.strictEqual(code, 1);
  }
));

// Test that preload can be used with --eval
common.spawnPromisified(nodeBinary, [...preloadOption(fixtureA), '-e', 'console.log("hello");']).then(common.mustCall(
  ({ stdout }) => {
    assert.strictEqual(stdout, 'A\nhello\n');
  }
));

// Test that preload can be used alongside other flags
{
  // Flags that would interfere with test output or require special configuration
  const excludedFlags = new Set([
    '--abort-on-uncaught-exception',
    '--build-snapshot',
    '--completion-bash',
    '--cpu-prof',
    '--enable-etw-stack-walking',
    '--enable-fips',
    '--force-fips',
    '--frozen-intrinsics',
    '--heap-prof',
    '--node-memory-debug',
    '--prof',
    '--prof-process',
    '--test',
    '--test-only',
    '--v8-options',
    '--watch',
    '--watch-preserve-output',
  ]);

  const helpText = childProcess.execFileSync(nodeBinary, ['--help'], { encoding: 'utf8' });
  const flagsToTest = helpText
    .split('\n')
    .filter((line) => line.startsWith('  --') && !line.includes('='))
    .map((line) => {
      const trimmed = line.trimStart();
      const end = trimmed.indexOf(' ');
      // Strip trailing comma that appears on wrapped help lines
      return (end === -1 ? trimmed : trimmed.slice(0, end)).replace(/,$/, '');
    })
    .filter((flag) => flag.length > 2 &&
      !flag.startsWith('--experimental-') &&
      // --allow-* flags require --permission and cannot be used standalone
      !flag.startsWith('--allow-') &&
      !excludedFlags.has(flag));

  for (const flag of flagsToTest) {
    common.spawnPromisified(nodeBinary, [flag, ...preloadOption(fixtureE), fixtureF]).then(common.mustCall(
      ({ stdout }) => {
        assert.strictEqual(stdout, 'smoosh\n');
      }
    ));
  }
}

// Test that preload can be used with stdin
const stdinProc = childProcess.spawn(
  nodeBinary,
  ['--require', fixtureA],
  { stdio: 'pipe' }
);
stdinProc.stdin.end("console.log('hello');");
let stdinStdout = '';
stdinProc.stdout.on('data', function(d) {
  stdinStdout += d;
});
stdinProc.on('close', common.mustCall((code) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(stdinStdout, 'A\nhello\n');
}));

// Test that preload can be used with repl
const replProc = childProcess.spawn(
  nodeBinary,
  ['-i', '--require', fixtureA],
  { stdio: 'pipe' }
);
replProc.stdin.end('.exit\n');
let replStdout = '';
replProc.stdout.on('data', (d) => {
  replStdout += d;
});
replProc.on('close', common.mustCall((code) => {
  assert.strictEqual(code, 0);
  const output = [
    'A',
    '> ',
  ];
  assert.ok(replStdout.startsWith(output[0]));
  assert.ok(replStdout.endsWith(output[1]));
}));

// Test that preload placement at other points in the cmdline
// also test that duplicated preload only gets loaded once
common.spawnPromisified(nodeBinary, [
  ...preloadOption(fixtureA),
  '-e', 'console.log("hello");',
  ...preloadOption(fixtureA, fixtureB),
]).then(common.mustCall(
  ({ stdout }) => {
    assert.strictEqual(stdout, 'A\nB\nhello\n');
  }
));

// Test that preload works with -i
const interactive = childProcess.spawn(nodeBinary, [...preloadOption(fixtureD), '-i']);

{
  const stdout = [];
  interactive.stdout.on('data', (chunk) => {
    stdout.push(chunk);
  });
  interactive.on('close', common.mustCall(() => {
    assert.match(Buffer.concat(stdout).toString('utf8'), /> 'test'\r?\n> $/);
  }));
}

interactive.stdin.write('a\n');
interactive.stdin.write('process.exit()\n');

common.spawnPromisified(nodeBinary,
                        ['--require', fixtures.path('cluster-preload.js'), fixtures.path('cluster-preload-test.js')])
  .then(common.mustCall(({ stdout }) => {
    assert.match(stdout, /worker terminated with code 43/);
  }));

// Test that preloading with a relative path works
common.spawnPromisified(nodeBinary,
                        [...preloadOption('./printA.js'), fixtureB],
                        { cwd: fixtures.fixturesDir }).then(common.mustCall(
  ({ stdout }) => {
    assert.strictEqual(stdout, 'A\nB\n');
  })
);
if (common.isWindows) {
  // https://github.com/nodejs/node/issues/21918
  common.spawnPromisified(nodeBinary,
                          [...preloadOption('.\\printA.js'), fixtureB],
                          { cwd: fixtures.fixturesDir }).then(common.mustCall(
    ({ stdout }) => {
      assert.strictEqual(stdout, 'A\nB\n');
    })
  );
}

// https://github.com/nodejs/node/issues/1691
common.spawnPromisified(nodeBinary,
                        ['--require', fixtures.path('cluster-preload.js'), 'cluster-preload-test.js'],
                        { cwd: fixtures.fixturesDir }).then(common.mustCall(
  ({ stdout }) => {
    assert.match(stdout, /worker terminated with code 43/);
  }
));
