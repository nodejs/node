'use strict';

const common = require('../common');
const stream = require('stream');

function testPushArg(val) {
  const readable = new stream.Readable({
    read: () => {}
  });
  readable.on('error', common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  }));
  readable.push(val);
}

testPushArg([]);
testPushArg({});
testPushArg(0);

function testUnshiftArg(val) {
  const readable = new stream.Readable({
    read: () => {}
  });
  readable.on('error', common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  }));
  readable.unshift(val);
}

testUnshiftArg([]);
testUnshiftArg({});
testUnshiftArg(0);
