'use strict';
// Flags: --expose-internals --experimental-repl-await

require('../common');
const ArrayStream = require('../common/arraystream');

const assert = require('assert');

function* expectedLines(lines) {
  yield* lines;
}

const expectedDebug = expectedLines([
  [
    'line %j',
    'const x = await new Promise((r) => setTimeout(() => r(1), 500));',
  ],
  [
    'eval %j',
    'const x = await new Promise((r) => setTimeout(() => r(1), 500));\n',
  ],
  ['queuing line %j', 'x;'],
  ['finish', null, undefined],
  ['queued line %j', 'x;'],
  ['eval %j', 'x;\n'],
  ['finish', null, 1],
  ['line %j', 'const y = 3;'],
  ['eval %j', 'const y = 3;\n'],
  ['finish', null, undefined],
  ['line %j', 'x + y;'],
  ['eval %j', 'x + y;\n'],
  ['finish', null, 4],
  ['line %j', 'const z = 4;'],
  ['eval %j', 'const z = 4;\n'],
  ['finish', null, undefined],
  ['queuing line %j', 'z + 1'],
  ['queued line %j', 'z + 1'],
  ['eval %j', 'z + 1\n'],
  ['finish', null, 5],
]);

let calledDebug = false;
require('internal/util/debuglog').debuglog = () => {
  return (...args) => {
    calledDebug = true;
    assert.deepStrictEqual(args, expectedDebug.next().value);
  };
};

// Import `repl` after replacing original `debuglog`
const repl = require('repl');

const putIn = new ArrayStream();
repl.start({
  input: putIn,
  output: putIn,
  useGlobal: false,
});

const expectedOutput = expectedLines([
  'undefined\n',
  '> ',
  '1\n',
  '> ',
  'undefined\n',
  '> ',
  '4\n',
  '> ',
  'undefined\n',
  '> ',
  '5\n',
  '> ',
]);

let calledOutput = false;
function writeCallback(data) {
  calledOutput = true;
  assert.strictEqual(data, expectedOutput.next().value);
}

putIn.write = writeCallback;

function clearCalled() {
  calledDebug = false;
  calledOutput = false;
}

// Lines sent after an async command that hasn't finished
// in the same write are enqueued
function test1() {
  putIn.run([
    'const x = await new Promise((r) => setTimeout(() => r(1), 500));\nx;',
  ]);
  assert(calledDebug);

  setTimeout(() => {
    assert(calledOutput);
    clearCalled();

    test2();
  }, 1000);
}

// Lines sent after a sync command in the same write are not enqueued
function test2() {
  putIn.run(['const y = 3;\nx + y;']);

  assert(calledDebug);
  assert(calledOutput);
  clearCalled();

  test3();
}

// Lines sent during an output write event of a previous command
// are enqueued
function test3() {
  putIn.write = (data) => {
    writeCallback(data);
    putIn.write = writeCallback;
    putIn.run(['z + 1']);
  };

  putIn.run(['const z = 4;']);

  assert(calledDebug);
  assert(calledOutput);
}

test1();
