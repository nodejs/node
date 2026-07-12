'use strict';

require('../common');

const assert = require('assert');
const { URLPattern } = require('url');

// Verify that if an error is thrown while accessing any of the
// init options, the error is appropriately propagated.
assert.throws(() => {
  new URLPattern({
    get protocol() {
      throw new Error('boom');
    }
  });
}, {
  message: 'boom',
});

// Verify that if an error is thrown while accessing the ignoreCase
// option, the error is appropriately propagated.
assert.throws(() => {
  new URLPattern({}, { get ignoreCase() {
    throw new Error('boom');
  } });
}, {
  message: 'boom'
});

{
  const result = new URLPattern({ pathname: '/:value' })
    .exec('https://example.com/test');

  assert.deepStrictEqual(Object.keys(result), [
    'hash',
    'hostname',
    'inputs',
    'password',
    'pathname',
    'port',
    'protocol',
    'search',
    'username',
  ]);
  assert.deepStrictEqual(Object.keys(result.pathname), [
    'groups',
    'input',
  ]);
  assert.strictEqual(result.hostname.input, 'example.com');
  assert.strictEqual(result.pathname.input, '/test');
  assert.strictEqual(result.pathname.groups.value, 'test');
}
