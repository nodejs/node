const test = require('node:test');
const assert = require('node:assert');
const { foo } = require('../logic-file.js');

test('foo returns 1 from a dotfile test', () => {
  assert.strictEqual(foo(), 1);
});
