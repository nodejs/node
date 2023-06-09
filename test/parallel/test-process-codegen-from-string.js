'use strict';

const common = require('../common');
const assert = require('assert');

const item = { foo: 0 };
eval('++item.foo');

assert.strictEqual(item.foo, 1);

// Basic eval() test
process.on('codeGenerationFromString', common.mustCall((code) => {
  assert.strictEqual(code, 'item.foo++');
}));

eval('item.foo++');
assert.strictEqual(item.foo, 2);

process.removeAllListeners('codeGenerationFromString');

// Basic Function() test
process.on('codeGenerationFromString', common.mustCall((code) => {
  assert.strictEqual(code, '(function anonymous(a,b\n) {\nreturn a + b\n})');
}));

const fct = new Function('a', 'b', 'return a + b');
assert.strictEqual(fct(1, 2), 3);

process.removeAllListeners('codeGenerationFromString');
