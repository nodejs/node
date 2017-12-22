'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

const exitTests = [];
process.on('exit', () => {
  for (const test of exitTests) test();
});
const options = {
  eval: common.mustCall((cmd, context) => {
    // Assertions here will not cause the test to exit with an error code
    // so set a boolean that is checked later instead.
    exitTests.push(common.mustCall(() => {
      assert.strictEqual(cmd, 'function f() {}\n');
      assert.strictEqual(context.foo, 'bar');
    }));
  })
};

const r = repl.start(options);
r.context = { foo: 'bar' };

try {
  // Default preprocessor transforms
  //  function f() {}  to
  //  var f = function f() {}
  // Test to ensure that original input is preserved.
  // Reference: https://github.com/nodejs/node/issues/9743
  r.write('function f() {}\n');
} finally {
  r.write('.exit\n');
}
