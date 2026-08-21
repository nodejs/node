'use strict';

const assert = require('assert');
const fs = require('fs');

// Verify invalid paths throw synchronously and return a Promise
(async () => {
  let promise;
  assert.doesNotThrow(() => { promise = fs.openAsBlob('does-not-exist'); });
  assert.strictEqual(typeof promise?.then, 'function');
  await assert.rejects(promise, { code: 'ERR_INVALID_ARG_VALUE' });
})();


// Verify that invalid arguments throw synchronously and return a Promise
(async () => {
  let promise;
  assert.doesNotThrow(() => { promise = fs.openAsBlob(123); });
  assert.strictEqual(typeof promise?.then, 'function');
  await assert.rejects(promise, { code: 'ERR_INVALID_ARG_TYPE' });
})();