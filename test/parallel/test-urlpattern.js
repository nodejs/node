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

{
  const accessed = [];
  const expected = [
    'baseURL',
    'hash',
    'hostname',
    'password',
    'pathname',
    'port',
    'protocol',
    'search',
    'username',
  ];
  const init = new Proxy({}, {
    get(target, name, receiver) {
      accessed.push(name);
      return Reflect.get(target, name, receiver);
    },
  });

  new URLPattern(init);
  assert.deepStrictEqual(accessed, expected);
}

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

{
  const input = new URLPattern({ pathname: '/x' })
    .exec({
      protocol: 'https',
      pathname: '/x',
      username: undefined,
    }).inputs[0];

  assert.deepStrictEqual(Object.keys(input), ['pathname', 'protocol']);
  assert.strictEqual('username' in input, false);
  assert.deepStrictEqual({ ...input }, {
    pathname: '/x',
    protocol: 'https',
  });
}
