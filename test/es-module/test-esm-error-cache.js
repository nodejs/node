'use strict';

const common = require('../common');
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
      // The module may be compiled again and a new SyntaxError would be thrown but
      // with the same content.
      assert.deepStrictEqual(error, e);
      return true;
    }
  );
})().then(common.mustCall());
