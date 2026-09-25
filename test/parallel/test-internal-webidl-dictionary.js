'use strict';

// Flags: --expose-internals

const common = require('../common');
const assert = require('node:assert/strict');
const { createDictionaryConverter } = require('internal/webidl');

const groups = [
  [{ key: 'z', converter: String }, { key: 'b', converter: String }],
  [{ key: 'c', converter: String }, { key: 'a', converter: String }],
];
let convert;
Object.defineProperty(Array.prototype, 2, {
  configurable: true,
  get: common.mustNotCall(),
  set: common.mustNotCall(),
});
try {
  convert = createDictionaryConverter('Derived', groups);
} finally {
  delete Array.prototype[2];
}

const first = convert(null);
const second = convert(undefined, new Proxy({}, { get: common.mustNotCall() }));
assert.deepStrictEqual(first, { __proto__: null });
assert.deepStrictEqual(second, first);
assert.notStrictEqual(first, second);

const trace = [];
const input = {
  __proto__: { c: 'inherited' },
  b: {
    toString: common.mustCall(() => {
      trace.push('convert b');
      input.z = 'changed';
      return 'b';
    }),
  },
  z: 'original',
  a: 'a',
};
const result = convert(new Proxy(input, {
  get(target, key) { trace.push(key); return target[key]; },
}));
assert.deepStrictEqual(trace, ['b', 'convert b', 'z', 'a', 'c']);
assert.deepStrictEqual(result, { __proto__: null, b: 'b', z: 'changed', a: 'a', c: 'inherited' });

trace.length = 0;
convert(new Proxy({}, { get(target, key) { trace.push(key); } }));
assert.deepStrictEqual(trace, ['b', 'z', 'a', 'c']);

const defaults = createDictionaryConverter('Defaults', [[{
  key: 'z', converter: common.mustNotCall(), defaultValue: common.mustCall(() => [], 2),
}]]);
assert.notStrictEqual(defaults(null).z, defaults(undefined).z);

const required = createDictionaryConverter('Required', [[{
  key: 'z', converter: common.mustNotCall(), defaultValue: common.mustCall(() => 'default', 2),
}], [
  { key: 'a', converter: common.mustNotCall(), required: true },
  { key: 'b', converter: common.mustNotCall(), defaultValue: common.mustNotCall() },
]]);
for (const value of [null, undefined]) {
  assert.throws(() => required(value), { code: 'ERR_MISSING_OPTION' });
}
