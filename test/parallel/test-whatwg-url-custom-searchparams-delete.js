'use strict';

// Tests below are not from WPT.

const common = require('../common');
const assert = require('assert');
const { URL, URLSearchParams } = require('url');

{
  const params = new URLSearchParams();
  common.expectsError(() => {
    params.delete.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type URLSearchParams'
  });
  common.expectsError(() => {
    params.delete();
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
  assert.throws(() => params.delete(obj), /^Error: toString$/);
  assert.throws(() => params.delete(sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
}

// https://github.com/nodejs/node/issues/10480
// Emptying searchParams should correctly update url's query
{
  const url = new URL('http://domain?var=1&var=2&var=3');
  for (const param of url.searchParams.keys()) {
    url.searchParams.delete(param);
  }
  assert.strictEqual(url.searchParams.toString(), '');
  assert.strictEqual(url.search, '');
  assert.strictEqual(url.href, 'http://domain/');
}
