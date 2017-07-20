'use strict';
const assert = require('assert');
const common = require('../common');
const OutgoingMessage = require('_http_outgoing').OutgoingMessage;

// Verify that an error is thrown upon a call to `OutgoingMessage.pipe`.

const outgoingMessage = new OutgoingMessage();
assert.throws(
  common.mustCall(() => { outgoingMessage.pipe(outgoingMessage); }),
  (err) => {
    return ((err instanceof Error) && /Cannot pipe, not readable/.test(err));
  },
  'OutgoingMessage.pipe should throw an error'
);
