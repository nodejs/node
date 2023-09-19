const assert = require('assert');
module.exports = 'main';

// Test self-resolve
require('@exports/sugar');

// Test self-resolve dynamic import
import('@exports/sugar').then(impt => {
  assert.strictEqual(impt.default, 'main');
});
