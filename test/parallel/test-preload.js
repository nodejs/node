'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
// Refs: https://github.com/nodejs/node/pull/2253
if (common.isSunOS)
  common.skip('unreliable on SunOS');

const assert = require('assert');
const childProcess = require('child_process');

const nodeBinary = process.argv[0];

const preloadOption = (preloads) => {
  let option = '';
  preloads.forEach(function(preload, index) {
    option += `-r "${preload}" `;
  });
  return option;
};

const fixtureA = fixtures.path('printA.js');
const fixtureB = fixtures.path('printB.js');
const fixtureC = fixtures.path('printC.js');
const fixtureD = fixtures.path('define-global.js');
const fixtureE = fixtures.path('intrinsic-mutation.js');
const fixtureF = fixtures.path('print-intrinsic-mutation-name.js');
const fixtureG = fixtures.path('worker-from-argv.js');
const fixtureThrows = fixtures.path('throws_error4.js');

// Test preloading a single module works
childProcess.exec(`"${nodeBinary}" ${preloadOption([fixtureA])} "${fixtureB}"`,
                  function(err, stdout, stderr) {
                    assert.ifError(err);
                    assert.strictEqual(stdout, 'A\nB\n');
                  });

// Test preloading multiple modules works
childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureA, fixtureB])} "${fixtureC}"`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'A\nB\nC\n');
  }
);

// Test that preloading a throwing module aborts
childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureA, fixtureThrows])} "${fixtureB}"`,
  function(err, stdout, stderr) {
    if (err) {
      assert.strictEqual(stdout, 'A\n');
    } else {
      throw new Error('Preload should have failed');
    }
  }
);

// Test that preload can be used with --eval
childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureA])}-e "console.log('hello');"`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'A\nhello\n');
  }
);

// Test that preload can be used with --frozen-intrinsics
childProcess.exec(
  `"${nodeBinary}" --frozen-intrinsics ${
    preloadOption([fixtureE])
  } ${
    fixtureF
  }`,
  function(err, stdout) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'smoosh\n');
  }
);
childProcess.exec(
  `"${
    nodeBinary
  }" --frozen-intrinsics ${
    preloadOption([fixtureE])
  } ${
    fixtureG
  } ${fixtureF}`,
  function(err, stdout) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'smoosh\n');
  }
);

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
stdinProc.on('close', function(code) {
  assert.strictEqual(code, 0);
  assert.strictEqual(stdinStdout, 'A\nhello\n');
});

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
replProc.on('close', function(code) {
  assert.strictEqual(code, 0);
  const output = [
    'A',
    '> '
  ];
  assert.ok(replStdout.startsWith(output[0]));
  assert.ok(replStdout.endsWith(output[1]));
});

// Test that preload placement at other points in the cmdline
// also test that duplicated preload only gets loaded once
childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureA])}-e "console.log('hello');" ${
    preloadOption([fixtureA, fixtureB])}`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'A\nB\nhello\n');
  }
);

// Test that preload works with -i
const interactive = childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureD])}-i`,
  common.mustCall(function(err, stdout, stderr) {
    assert.ifError(err);
    assert.ok(stdout.endsWith("> 'test'\n> "));
  })
);

interactive.stdin.write('a\n');
interactive.stdin.write('process.exit()\n');

childProcess.exec(
  `"${nodeBinary}" --require "${fixtures.path('cluster-preload.js')}" "${
    fixtures.path('cluster-preload-test.js')}"`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.ok(/worker terminated with code 43/.test(stdout));
  }
);

// Test that preloading with a relative path works
childProcess.exec(
  `"${nodeBinary}" ${preloadOption(['./printA.js'])} "${fixtureB}"`,
  { cwd: fixtures.fixturesDir },
  common.mustCall(function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'A\nB\n');
  })
);
if (common.isWindows) {
  // https://github.com/nodejs/node/issues/21918
  childProcess.exec(
    `"${nodeBinary}" ${preloadOption(['.\\printA.js'])} "${fixtureB}"`,
    { cwd: fixtures.fixturesDir },
    common.mustCall(function(err, stdout, stderr) {
      assert.ifError(err);
      assert.strictEqual(stdout, 'A\nB\n');
    })
  );
}

// https://github.com/nodejs/node/issues/1691
childProcess.exec(
  `"${nodeBinary}" --require ` +
     `"${fixtures.path('cluster-preload.js')}" cluster-preload-test.js`,
  { cwd: fixtures.fixturesDir },
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.ok(/worker terminated with code 43/.test(stdout));
  }
);
