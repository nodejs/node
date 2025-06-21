'use strict';
require('../common');
const assert = require('assert');

const { MessageChannel, MessagePort } = require('worker_threads');

// Make sure that `MessagePort` is the constructor for MessagePort instances,
// but not callable.
const { port1 } = new MessageChannel();

assert(port1 instanceof MessagePort);
assert.strictEqual(port1.constructor, MessagePort);

assert.throws(() => MessagePort(), {
  constructor: TypeError,
  code: 'ERR_CONSTRUCT_CALL_INVALID'
});

assert.throws(() => new MessagePort(), {
  constructor: TypeError,
  code: 'ERR_CONSTRUCT_CALL_INVALID'
});

assert.throws(() => MessageChannel(), {
  constructor: TypeError,
  code: 'ERR_CONSTRUCT_CALL_REQUIRED'
});
