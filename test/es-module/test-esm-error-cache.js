'use strict';

// Flags: --experimental-modules

const common = require('../common');
const assert = require('assert');

common.crashOnUnhandledRejection();

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
