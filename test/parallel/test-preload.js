'use strict';

const common = require('../common');
// Refs: https://github.com/nodejs/node/pull/2253
if (common.isSunOS)
  common.skip('unreliable on SunOS');

const assert = require('assert');
const path = require('path');
const childProcess = require('child_process');

const nodeBinary = process.argv[0];

const preloadOption = (preloads) => {
  let option = '';
  preloads.forEach(function(preload, index) {
    option += `-r "${preload}" `;
  });
  return option;
};

const fixture = (name) => path.join(common.fixturesDir, name);

const fixtureA = fixture('printA.js');
const fixtureB = fixture('printB.js');
const fixtureC = fixture('printC.js');
const fixtureD = fixture('define-global.js');
const fixtureThrows = fixture('throws_error4.js');

// test preloading a single module works
childProcess.exec(`"${nodeBinary}" ${preloadOption([fixtureA])} "${fixtureB}"`,
                  function(err, stdout, stderr) {
                    assert.ifError(err);
                    assert.strictEqual(stdout, 'A\nB\n');
                  });

// test preloading multiple modules works
childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureA, fixtureB])} "${fixtureC}"`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'A\nB\nC\n');
  }
);

// test that preloading a throwing module aborts
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

// test that preload can be used with --eval
childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureA])}-e "console.log('hello');"`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'A\nhello\n');
  }
);

// test that preload can be used with stdin
const stdinProc = childProcess.spawn(
  nodeBinary,
  ['--require', fixtureA],
  {stdio: 'pipe'}
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

// test that preload can be used with repl
const replProc = childProcess.spawn(
  nodeBinary,
  ['-i', '--require', fixtureA],
  {stdio: 'pipe'}
);
replProc.stdin.end('.exit\n');
let replStdout = '';
replProc.stdout.on('data', function(d) {
  replStdout += d;
});
replProc.on('close', function(code) {
  assert.strictEqual(code, 0);
  const output = [
    'A',
    '> '
  ].join('\n');
  assert.strictEqual(replStdout, output);
});

// test that preload placement at other points in the cmdline
// also test that duplicated preload only gets loaded once
childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureA])}-e "console.log('hello');" ${
    preloadOption([fixtureA, fixtureB])}`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, 'A\nB\nhello\n');
  }
);

// test that preload works with -i
const interactive = childProcess.exec(
  `"${nodeBinary}" ${preloadOption([fixtureD])}-i`,
  common.mustCall(function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, "> 'test'\n> ");
  })
);

interactive.stdin.write('a\n');
interactive.stdin.write('process.exit()\n');

childProcess.exec(
  `"${nodeBinary}" --require "${fixture('cluster-preload.js')}" "${
    fixture('cluster-preload-test.js')}"`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.ok(/worker terminated with code 43/.test(stdout));
  }
);

// https://github.com/nodejs/node/issues/1691
process.chdir(common.fixturesDir);
childProcess.exec(
  `"${nodeBinary}" --expose_natives_as=v8natives --require ` +
     `"${fixture('cluster-preload.js')}" cluster-preload-test.js`,
  function(err, stdout, stderr) {
    assert.ifError(err);
    assert.ok(/worker terminated with code 43/.test(stdout));
  }
);
