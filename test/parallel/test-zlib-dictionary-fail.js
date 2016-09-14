'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

// String "test" encoded with dictionary "dict".
const input = Buffer.from([0x78, 0xBB, 0x04, 0x09, 0x01, 0xA5]);

{
  const stream = zlib.createInflate();

  stream.on('error', common.mustCall(function(err) {
    assert(/Missing dictionary/.test(err.message));
  }));

  stream.write(input);
}

{
  const stream = zlib.createInflate({ dictionary: Buffer.from('fail') });

  stream.on('error', common.mustCall(function(err) {
    assert(/Bad dictionary/.test(err.message));
  }));

  stream.write(input);
}

{
  const stream = zlib.createInflateRaw({ dictionary: Buffer.from('fail') });

  stream.on('error', common.mustCall(function(err) {
    // It's not possible to separate invalid dict and invalid data when using
    // the raw format
    assert(/invalid/.test(err.message));
  }));

  stream.write(input);
}
