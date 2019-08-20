'use strict';

// Tests below are not from WPT.

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;

{
  const params = new URLSearchParams();
  common.expectsError(() => {
    params.getAll.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type URLSearchParams'
  });
  common.expectsError(() => {
    params.getAll();
  }, {
    code: 'ERR_MISSING_ARGS',
    type: TypeError,
    message: 'The "name" argument must be specified'
  });

  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  assert.throws(() => params.getAll(obj), /^Error: toString$/);
  assert.throws(() => params.getAll(sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
}
