'use strict';

// Used to test that `uncaughtException` is emitted

const { callbackify } = require('util');

{
  async function fn() { }

  const cbFn = callbackify(fn);

  cbFn((err, ret) => {
    throw new Error(__filename);
  });
}
