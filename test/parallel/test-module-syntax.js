var common = require('../common');
var assert = require('assert');

try {
  require('../fixtures/module-syntax-error.js');
} catch (e) {
  assert.equal(e.name, 'SyntaxError');
  assert.equal(e.message, 'Unexpected token }');
  return;
}
assert.ok(false, 'should not ok for code with syntax error');
