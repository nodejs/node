'use strict';

const common = require('../common');
const assert = require('assert');
const Duplex = require('stream').Duplex;

{
  const stream = new Duplex({
    read() {}
  });
  assert.strictEqual(stream.allowHalfOpen, true);
  stream.on('finish', common.mustNotCall());
  assert.strictEqual(stream.listenerCount('end'), 0);
  stream.resume();
  stream.push(null);
}

{
  const stream = new Duplex({
    read() {},
    allowHalfOpen: false
  });
  assert.strictEqual(stream.allowHalfOpen, false);
  stream.on('finish', common.mustCall());
  assert.strictEqual(stream.listenerCount('end'), 1);
  stream.resume();
  stream.push(null);
}

{
  const stream = new Duplex({
    read() {},
    allowHalfOpen: false
  });
  assert.strictEqual(stream.allowHalfOpen, false);
  stream._writableState.ended = true;
  stream.on('finish', common.mustNotCall());
  assert.strictEqual(stream.listenerCount('end'), 1);
  stream.resume();
  stream.push(null);
}
