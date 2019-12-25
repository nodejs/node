'use strict';

// Tests below are not from WPT.

require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;

{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.has.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    name: 'TypeError',
    message: 'Value of "this" must be of type URLSearchParams'
  });
  assert.throws(() => {
    params.has();
  }, {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
    message: 'The "name" argument must be specified'
  });

  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  assert.throws(() => params.has(obj), /^Error: toString$/);
  assert.throws(() => params.has(sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
}
