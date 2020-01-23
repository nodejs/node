'use strict';

const common = require('../common');
const stream = require('stream');

function testWriteType(val, objectMode, code) {
  const writable = new stream.Writable({
    objectMode,
    write: () => {}
  });
  if (!code) {
    writable.on('error', common.mustNotCall());
  } else {
    writable.on('error', common.expectsError({
      code: code,
    }));
  }
  writable.write(val);
}

testWriteType([], false, 'ERR_INVALID_ARG_TYPE');
testWriteType({}, false, 'ERR_INVALID_ARG_TYPE');
testWriteType(0, false, 'ERR_INVALID_ARG_TYPE');
testWriteType(true, false, 'ERR_INVALID_ARG_TYPE');
testWriteType(0.0, false, 'ERR_INVALID_ARG_TYPE');
testWriteType(undefined, false, 'ERR_INVALID_ARG_TYPE');
testWriteType(null, false, 'ERR_STREAM_NULL_VALUES');

testWriteType([], true);
testWriteType({}, true);
testWriteType(0, true);
testWriteType(true, true);
testWriteType(0.0, true);
testWriteType(undefined, true);
testWriteType(null, true, 'ERR_STREAM_NULL_VALUES');
