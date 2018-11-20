'use strict';

// Tests below are not from WPT.

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;

{
  const params = new URLSearchParams();
  common.expectsError(() => {
    params.append.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type URLSearchParams'
  });
  common.expectsError(() => {
    params.append('a');
  }, {
    code: 'ERR_MISSING_ARGS',
    type: TypeError,
    message: 'The "name" and "value" arguments must be specified'
  });

  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  assert.throws(() => params.set(obj, 'b'), /^Error: toString$/);
  assert.throws(() => params.set('a', obj), /^Error: toString$/);
  assert.throws(() => params.set(sym, 'b'),
                /^TypeError: Cannot convert a Symbol value to a string$/);
  assert.throws(() => params.set('a', sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
}
