'use strict';

const assert = require('assert');
const { Worker } = require('worker_threads');

{
  [
    undefined,
    null,
    false,
    0,
    Symbol('test'),
    {},
    [],
    () => {},
  ].forEach((val) => {
    assert.throws(
      () => new Worker(val),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );
  });
}
