'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

common.skipIfInspectorDisabled();

// This test verifies that the V8 inspector API is usable in the REPL.

const { replServer, input, output } = startNewREPLServer();

input.run(['const myVariable = 42']);

replServer.complete('myVar', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['myVariable'], 'myVar']);
}));

input.run([
  'const inspector = require("inspector")',
  'const session = new inspector.Session()',
  'session.connect()',
  'session.post("Runtime.evaluate", { expression: "1 + 1" }, console.log)',
  'session.disconnect()',
]);

assert(output.accumulator.includes(
  "null { result: { type: 'number', value: 2, description: '2' } }"));
