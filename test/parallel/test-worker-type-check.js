'use strict';

const common = require('../common');
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
    () => {}
  ].forEach((val) => {
    common.expectsError(
      () => new Worker(val),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "filename" argument must be of type string. ' +
                 `Received type ${typeof val}`
      }
    );
  });
}
