'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

function close() { this.close(); }
net.Server().listen({ port: undefined }, close);
net.Server().listen({ port: '' + common.PORT }, close);

const regex1 = /^Port should be > 0 and < 65536/;
const regex2 = /^Invalid listen argument/;

[ 'nan',
  -1,
  123.456,
  0x10000,
  1 / 0,
  -1 / 0,
  '+Infinity',
  '-Infinity' ].forEach((port) => {
    assert.throws(() => {
      net.Server().listen({ port: port }, assert.fail);
    }, err => regex1.test(err.message));
  });

[null, true, false].forEach((port) => {
  assert.throws(() => {
    net.Server().listen({ port: port }, assert.fail);
  }, err => regex2.test(err.message));
});
