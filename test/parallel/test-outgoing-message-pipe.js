'use strict';
const common = require('../common');
const OutgoingMessage = require('_http_outgoing').OutgoingMessage;

// Verify that an error is thrown upon a call to `OutgoingMessage.pipe`.

const outgoingMessage = new OutgoingMessage();
common.expectsError(
  () => { outgoingMessage.pipe(outgoingMessage); },
  {
    code: 'ERR_STREAM_CANNOT_PIPE',
    type: Error
  }
);
