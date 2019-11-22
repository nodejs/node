'use strict';

require('../common');
const assert = require('assert');

const file = '../fixtures/syntax/bad_syntax.mjs';

let error;
(async () => {
  try {
    await import(file);
  } catch (e) {
    assert.strictEqual(e.name, 'SyntaxError');
    error = e;
  }

  assert(error);

  await assert.rejects(
    () => import(file),
    (e) => {
      assert.strictEqual(error, e);
      return true;
    }
  );
})();
