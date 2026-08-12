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

// Basic object — no indentation by default
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

// Caller controls indentation via JSON.stringify args
assert.strictEqual(
  captured(() => c.json({ foo: 'bar' }, null, 2)),
  '{\n  "foo": "bar"\n}\n'
);

// Replacer array
assert.strictEqual(
  captured(() => c.json({ foo: 'bar', baz: 1 }, ['foo'])),
  '{"foo":"bar"}\n'
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
