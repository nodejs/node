'use strict';
const common = require('../common');
const assert = require('assert');
const url = require('url');

// https://github.com/joyent/node/issues/568
[
  [undefined, 'undefined'],
  [null, 'null'],
  [true, 'type boolean'],
  [false, 'type boolean'],
  [0.0, 'type number'],
  [0, 'type number'],
  [[], 'instance of Array'],
  [{}, 'instance of Object'],
  [() => {}, 'type function'],
  [Symbol('foo'), 'type symbol']
].forEach(([val, type]) => {
  const error = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: `The "url" argument must be of type string. Received ${type}`
  });
  assert.throws(() => { url.parse(val); }, error);
});

assert.throws(() => { url.parse('http://%E0%A4%A@fail'); },
              /^URIError: URI malformed$/);
