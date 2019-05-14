// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');

const { outHeadersKey } = require('internal/http');
const { OutgoingMessage } = require('http');

const warn = 'OutgoingMessage.prototype._headers is deprecated';
common.expectWarning('DeprecationWarning', warn, 'DEP0066');

{
  // Tests for _headers get method
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage.getHeaders = common.mustCall();
  outgoingMessage._headers;
}

{
  // Tests for _headers set method
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage._headers = {
    host: 'risingstack.com',
    Origin: 'localhost'
  };

  assert.deepStrictEqual(
    Object.entries(outgoingMessage[outHeadersKey]),
    Object.entries({
      host: ['host', 'risingstack.com'],
      origin: ['Origin', 'localhost']
    }));
}
