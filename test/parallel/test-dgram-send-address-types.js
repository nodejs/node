'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const port = common.PORT;
const host = common.localhostIPv4;
const buf = Buffer.from('test');
const client = dgram.createSocket('udp4');

client.bind(0, common.mustCall(() => {
  const onMessage = common.mustCall((err, bytes) => {
    assert.ifError(err);
    assert.strictEqual(bytes, buf.length);
  }, 5);

  // valid address: false
  client.send(buf, port, false, onMessage);

  // valid address: empty string
  client.send(buf, port, '', onMessage);

  // valid address: null
  client.send(buf, port, null, onMessage);

  // valid address: 0
  client.send(buf, port, 0, onMessage);

  // valid address: undefined
  client.send(buf, port, undefined, onMessage);

  // the socket must be bound before these following assertions;
  // otherwise the exceptions won't be thrown synchronously.

  // invalid address: object
  assert.throws(() => {
    client.send(buf, port, []);
  }, TypeError);

  // invalid address: nonzero number
  assert.throws(() => {
    client.send(buf, port, 1);
  }, TypeError);

  // invalid address: true
  assert.throws(() => {
    client.send(buf, port, true);
  }, TypeError);

  // invalid address: function
  assert.throws(() => {
    client.send(buf, port, () => {
    });
  }, TypeError);
}))
  .unref();
