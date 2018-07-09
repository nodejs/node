// Flags: --experimental-worker
'use strict';
const common = require('../common');

// This test checks that Worker has correct exit codes on parent side
// in multiple situations.

const assert = require('assert');
const worker = require('worker_threads');
const { Worker, isMainThread, parentPort } = worker;

if (isMainThread) {
  parent();
} else {
  if (!parentPort) {
    console.error('Parent port must not be null');
    process.exit(100);
    return;
  }
  parentPort.once('message', (msg) => {
    switch (msg) {
      case 'child1':
        return child1();
      case 'child2':
        return child2();
      case 'child3':
        return child3();
      case 'child4':
        return child4();
      case 'child5':
        return child5();
      case 'child6':
        return child6();
      case 'child7':
        return child7();
      default:
        throw new Error('invalid');
    }
  });
}

function child1() {
  process.exitCode = 42;
  process.on('exit', (code) => {
    assert.strictEqual(code, 42);
  });
}

function child2() {
  process.exitCode = 99;
  process.on('exit', (code) => {
    assert.strictEqual(code, 42);
  });
  process.exit(42);
}

function child3() {
  process.exitCode = 99;
  process.on('exit', (code) => {
    assert.strictEqual(code, 0);
  });
  process.exit(0);
}

function child4() {
  process.exitCode = 99;
  process.on('exit', (code) => {
    // cannot use assert because it will be uncaughtException -> 1 exit code
    // that will render this test useless
    if (code !== 1) {
      console.error('wrong code! expected 1 for uncaughtException');
      process.exit(99);
    }
  });
  throw new Error('ok');
}

function child5() {
  process.exitCode = 95;
  process.on('exit', (code) => {
    assert.strictEqual(code, 95);
    process.exitCode = 99;
  });
}

function child6() {
  process.on('exit', (code) => {
    assert.strictEqual(code, 0);
  });
  process.on('uncaughtException', common.mustCall(() => {
    // handle
  }));
  throw new Error('ok');
}

function child7() {
  process.on('exit', (code) => {
    assert.strictEqual(code, 97);
  });
  process.on('uncaughtException', common.mustCall(() => {
    process.exitCode = 97;
  }));
  throw new Error('ok');
}

function parent() {
  const test = (arg, exit, error = null) => {
    const w = new Worker(__filename);
    w.on('exit', common.mustCall((code) => {
      assert.strictEqual(
        code, exit,
        `wrong exit for ${arg}\nexpected:${exit} but got:${code}`);
      console.log(`ok - ${arg} exited with ${exit}`);
    }));
    if (error) {
      w.on('error', common.mustCall((err) => {
        assert(error.test(err));
      }));
    }
    w.postMessage(arg);
  };

  test('child1', 42);
  test('child2', 42);
  test('child3', 0);
  test('child4', 1, /^Error: ok$/);
  test('child5', 99);
  test('child6', 0);
  test('child7', 97);
}
