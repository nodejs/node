const assert = require('assert');

class Parent {}
class A extends Parent {}

module.exports = A;
require('./warning-moduleexports-class-b.js');
process.nextTick(() => {
  assert.strictEqual(module.exports, A);
  assert.strictEqual(Object.getPrototypeOf(module.exports), Parent);
});
