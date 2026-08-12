'use strict';
require('../common');

const assert = require('assert');
const { Console } = require('console');

// Capture stdout into a string
let output = '';
const mockStdout = {
  write(chunk) { output += chunk; },
  isTTY: false,
};

const c = new Console(mockStdout);

function captured(fn) {
  output = '';
  fn();
  return output;
}

// Basic object
assert.strictEqual(
  captured(() => c.json({ foo: 'bar' })),
  '{\n  "foo": "bar"\n}\n'
);

// Null-prototype object — the motivating case
const nullProto = Object.assign(Object.create(null), { foo: 'bar' });
assert.strictEqual(
  captured(() => c.json(nullProto)),
  '{\n  "foo": "bar"\n}\n'
);

// Array
assert.strictEqual(
  captured(() => c.json([1, 2, 3])),
  '[\n  1,\n  2,\n  3\n]\n'
);

// Primitive values
assert.strictEqual(captured(() => c.json(42)), '42\n');
assert.strictEqual(captured(() => c.json('hello')), '"hello"\n');
assert.strictEqual(captured(() => c.json(null)), 'null\n');
assert.strictEqual(captured(() => c.json(true)), 'true\n');

// Multiple args — each printed separately
assert.strictEqual(
  captured(() => c.json({ a: 1 }, { b: 2 })),
  '{\n  "a": 1\n}\n{\n  "b": 2\n}\n'
);

// No args — no output
assert.strictEqual(captured(() => c.json()), '');

// Circular reference throws TypeError
const circular = {};
circular.self = circular;
assert.throws(() => c.json(circular), TypeError);

// Non-serializable top-level values (undefined, functions, symbols) are silently
// skipped, consistent with JSON.stringify returning undefined for them.
assert.strictEqual(captured(() => c.json(undefined)), '');
assert.strictEqual(captured(() => c.json(() => {})), '');
assert.strictEqual(captured(() => c.json(Symbol('x'))), '');
