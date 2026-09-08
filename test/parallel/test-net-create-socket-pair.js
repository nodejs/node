'use strict';
const assert = require('node:assert');
const { once } = require('node:events');
const { text } = require('node:stream/consumers');
const { testCreateSocketPair } = require('../common/net-create-socket-pair');

testCreateSocketPair('createSocketPair returns connected duplex sockets',
  (left, right) => {
    assert.strictEqual(left.readable, true);
    assert.strictEqual(left.writable, true);
    assert.strictEqual(right.readable, true);
    assert.strictEqual(right.writable, true);

    left.end();
    right.end();
  });

testCreateSocketPair('socket pair endpoints exchange bytes in both directions',
  async (left, right) => {
    const leftData = once(left, 'data');
    const rightData = once(right, 'data');

    left.write('ping');
    right.write('pong');

    assert.strictEqual((await leftData)[0].toString(), 'pong');
    assert.strictEqual((await rightData)[0].toString(), 'ping');

    left.end();
    right.end();
  });

testCreateSocketPair('one endpoint can finish before the other endpoint writes',
  async (left, right) => {
    const leftOutput = text(left);
    const rightOutput = text(right);

    left.end('from left');
    right.end('from right');

    assert.strictEqual(await leftOutput, 'from right');
    assert.strictEqual(await rightOutput, 'from left');
  });
