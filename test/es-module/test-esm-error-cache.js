'use strict';

// Flags: --experimental-modules

require('../common');
const assert = require('assert');

const file = '../fixtures/syntax/bad_syntax.js';

let error;
(async () => {
  try {
    await import(file);
  } catch (e) {
    assert.strictEqual(e.name, 'SyntaxError');
    error = e;
  }

  assert(error);

  try {
    await import(file);
  } catch (e) {
    assert.strictEqual(error, e);
  }
})();
