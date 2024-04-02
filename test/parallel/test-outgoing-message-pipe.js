'use strict';
require('../common');
const assert = require('assert');
const OutgoingMessage = require('_http_outgoing').OutgoingMessage;

// Verify that an error is thrown upon a call to `OutgoingMessage.pipe`.

const outgoingMessage = new OutgoingMessage();
assert.throws(
  () => { outgoingMessage.pipe(outgoingMessage); },
  {
    code: 'ERR_STREAM_CANNOT_PIPE',
    name: 'Error'
  }
);
