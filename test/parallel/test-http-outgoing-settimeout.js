'use strict';
const common = require('../common');
const assert = require('assert');

const { OutgoingMessage } = require('http');

{
  // tests for settimeout method with socket
  const expectedMsecs = 42;
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.socket = {
    setTimeout: common.mustCall((msecs) => {
      assert.strictEqual(msecs, expectedMsecs);
    })
  };
  outgoingMessage.setTimeout(expectedMsecs);
}

{
  // tests for settimeout method without socket
  const expectedMsecs = 23;
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.setTimeout(expectedMsecs);

  outgoingMessage.emit('socket', {
    setTimeout: common.mustCall((msecs) => {
      assert.strictEqual(msecs, expectedMsecs);
    })
  });
}
