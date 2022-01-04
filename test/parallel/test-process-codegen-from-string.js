'use strict';

const common = require('../common');
const assert = require('assert');

const item = { foo: 0 };
eval('++item.foo');

assert.strictEqual(item.foo, 1);

process.on('codeGenerationFromString', common.mustCall(handleError((code) => {
  assert.strictEqual(code, 'item.foo++');
})));

eval('item.foo++');
assert.strictEqual(item.foo, 2);

process.removeAllListeners('codeGenerationFromString');

process.on('codeGenerationFromString', common.mustCall(handleError((code) => {
  assert.strictEqual(code, '(function anonymous(a,b\n) {\nreturn a + b\n})');
})));

const fct = new Function('a', 'b', 'return a + b');
assert.strictEqual(fct(1, 2), 3);

function handleError(fn) {
  return (...args) => {
    try {
      fn(...args);
    } catch (err) {
      // The C++ code will just log the error to stderr and continue with the
      // flow of the program. Set the exit code manually to ensure the test
      // script fails in case of an error.
      process.exitCode = 1;
      throw err;
    }
  };
}
