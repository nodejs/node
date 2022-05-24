'use strict';

// Tests below are not from WPT.

require('../common');
const assert = require('assert');

{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.get.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    name: 'TypeError',
  });
  assert.throws(() => {
    params.get();
  }, {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
  });

  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  assert.throws(() => params.get(obj), /^Error: toString$/);
  assert.throws(() => params.get(sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
}
