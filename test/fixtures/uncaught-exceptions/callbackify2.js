'use strict';

// Used to test the `uncaughtException` err object

const assert = require('assert');
const { callbackify } = require('util');

{
  const sentinel = new Error(__filename);
  process.once('uncaughtException', (err) => {
    assert.strictEqual(err, sentinel);
    // Calling test will use `stdout` to assert value of `err.message`
    console.log(err.message);
  });

  async function fn() {
    return await Promise.reject(sentinel);
  }

  const cbFn = callbackify(fn);
  cbFn((err, ret) => assert.ifError(err));
}
