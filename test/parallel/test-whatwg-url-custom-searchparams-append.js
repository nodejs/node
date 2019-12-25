'use strict';

// Tests below are not from WPT.

require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;

{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.append.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    name: 'TypeError',
    message: 'Value of "this" must be of type URLSearchParams'
  });
  assert.throws(() => {
    params.append('a');
  }, {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
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
