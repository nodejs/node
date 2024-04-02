'use strict';

require('../common');

const { URL } = require('url');
const assert = require('assert');

[
  'toString',
  'toJSON',
].forEach((i) => {
  assert.throws(() => Reflect.apply(URL.prototype[i], [], {}), {
    code: 'ERR_INVALID_THIS',
  });
});

[
  'href',
  'protocol',
  'username',
  'password',
  'host',
  'hostname',
  'port',
  'pathname',
  'search',
  'hash',
].forEach((i) => {
  assert.throws(() => Reflect.get(URL.prototype, i, {}), {
    code: 'ERR_INVALID_THIS',
  });

  assert.throws(() => Reflect.set(URL.prototype, i, null, {}), {
    code: 'ERR_INVALID_THIS',
  });
});

[
  'origin',
  'searchParams',
].forEach((i) => {
  assert.throws(() => Reflect.get(URL.prototype, i, {}), {
    code: 'ERR_INVALID_THIS',
  });
});
