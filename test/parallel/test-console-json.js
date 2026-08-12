'use strict';
require('../common');

const assert = require('assert');
const { Console } = require('console');

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
  '{"foo":"bar"}\n'
);

// Null-prototype object — the motivating case
const nullProto = Object.assign(Object.create(null), { foo: 'bar' });
assert.strictEqual(
  captured(() => c.json(nullProto)),
  '{"foo":"bar"}\n'
);

// Multiple args — each printed on a separate line
assert.strictEqual(
  captured(() => c.json({ a: 1 }, { b: 2 })),
  '{"a":1}\n{"b":2}\n'
);

// Array
assert.strictEqual(
  captured(() => c.json([1, 2, 3])),
  '[1,2,3]\n'
);

// Primitives
assert.strictEqual(captured(() => c.json(42)), '42\n');
assert.strictEqual(captured(() => c.json('hello')), '"hello"\n');
assert.strictEqual(captured(() => c.json(null)), 'null\n');
assert.strictEqual(captured(() => c.json(true)), 'true\n');

// Circular reference throws TypeError
const circular = {};
circular.self = circular;
assert.throws(() => c.json(circular), TypeError);
