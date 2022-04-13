'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');
const path = require('path');
const testName = path.join(__dirname, 'test-http-max-http-headers.js');

const timeout = common.platformTimeout(100);

const tests = [];

function test(fn) {
  tests.push(fn);
}

test(function(cb) {
  console.log('running subtest expecting failure');

  // Validate that the test fails if the max header size is too small.
  const args = ['--expose-internals',
                '--max-http-header-size=1024',
                testName];
  const cp = spawn(process.execPath, args, { stdio: 'inherit' });

  cp.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    cb();
  }));
});

test(function(cb) {
  console.log('running subtest expecting success');

  const env = Object.assign({}, process.env, {
    NODE_DEBUG: 'http'
  });

  // Validate that the test now passes if the same limit is large enough.
  const args = ['--expose-internals',
                '--max-http-header-size=1024',
                testName,
                '1024'];
  const cp = spawn(process.execPath, args, {
    env,
    stdio: 'inherit'
  });

  cp.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    cb();
  }));
});

// Next, repeat the same checks using NODE_OPTIONS if it is supported.
if (!process.config.variables.node_without_node_options) {
  const env = Object.assign({}, process.env, {
    NODE_OPTIONS: '--max-http-header-size=1024'
  });

  test(function(cb) {
    console.log('running subtest expecting failure');

    // Validate that the test fails if the max header size is too small.
    const args = ['--expose-internals', testName];
    const cp = spawn(process.execPath, args, { env, stdio: 'inherit' });

    cp.on('close', common.mustCall((code, signal) => {
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
      cb();
    }));
  });

  test(function(cb) {
    // Validate that the test now passes if the same limit is large enough.
    const args = ['--expose-internals', testName, '1024'];
    const cp = spawn(process.execPath, args, { env, stdio: 'inherit' });

    cp.on('close', common.mustCall((code, signal) => {
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
      cb();
    }));
  });
}

function runTest() {
  const fn = tests.shift();

  if (!fn) {
    return;
  }

  fn(() => {
    setTimeout(runTest, timeout);
  });
}

runTest();
