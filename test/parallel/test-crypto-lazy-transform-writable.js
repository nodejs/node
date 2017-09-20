'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const Stream = require('stream');
const util = require('util');

const hasher1 = crypto.createHash('sha256');
const hasher2 = crypto.createHash('sha256');

// Calculate the expected result.
hasher1.write(Buffer.from('hello world'));
hasher1.end();

const expected = hasher1.read().toString('hex');

function OldStream() {
  Stream.call(this);

  this.readable = true;
}
util.inherits(OldStream, Stream);

const stream = new OldStream();

stream.pipe(hasher2).on('finish', common.mustCall(function() {
  const hash = hasher2.read().toString('hex');
  assert.strictEqual(expected, hash);
}));

stream.emit('data', Buffer.from('hello'));
stream.emit('data', Buffer.from(' world'));
stream.emit('end');
