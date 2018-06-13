'use strict';

const common = require('../common');
const assert = require('assert');
const Duplex = require('stream').Duplex;
{
  const stream = new Duplex();
  assert(stream.allowHalfOpen);
  stream.on('finish', common.mustNotCall());
  assert.strictEqual(stream.emit('end'), false);
}

{
  const stream = new Duplex({
    allowHalfOpen: false
  });
  assert.strictEqual(stream.allowHalfOpen, false);
  stream.on('finish', common.mustCall());
  assert(stream.emit('end'));
}

{
  const stream = new Duplex({
    allowHalfOpen: false
  });
  assert.strictEqual(stream.allowHalfOpen, false);
  stream._writableState.ended = true;
  stream.on('finish', common.mustNotCall());
  assert(stream.emit('end'));
}
