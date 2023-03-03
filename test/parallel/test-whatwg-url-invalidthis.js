'use strict';

require('../common');

const { URL } = require('url');
const assert = require('assert');

[
  'toString',
  'toJSON',
].forEach((i) => {
  assert.throws(() => Reflect.apply(URL.prototype[i], [], {}), {
    name: 'TypeError',
    message: /Cannot read private member/,
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
    name: 'TypeError',
    message: /Cannot read private member/,
  });

  assert.throws(() => Reflect.set(URL.prototype, i, null, {}), {
    name: 'TypeError',
    message: /Cannot read private member/,
  });
});

[
  'origin',
  'searchParams',
].forEach((i) => {
  assert.throws(() => Reflect.get(URL.prototype, i, {}), {
    name: 'TypeError',
    message: /Cannot read private member/,
  });
});
