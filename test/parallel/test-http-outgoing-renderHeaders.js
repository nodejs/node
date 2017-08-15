'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');

const outHeadersKey = require('internal/http').outHeadersKey;
const http = require('http');
const OutgoingMessage = http.OutgoingMessage;

{
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage._header = {};
  common.expectsError(
    outgoingMessage._renderHeaders.bind(outgoingMessage),
    {
      code: 'ERR_HTTP_HEADERS_SENT',
      type: Error,
      message: 'Cannot render headers after they are sent to the client'
    }
  );
}

{
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage[outHeadersKey] = null;
  const result = outgoingMessage._renderHeaders();
  assert.deepStrictEqual(result, {});
}


{
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage[outHeadersKey] = {};
  const result = outgoingMessage._renderHeaders();
  assert.deepStrictEqual(result, {});
}

{
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage[outHeadersKey] = {
    host: ['host', 'nodejs.org'],
    origin: ['Origin', 'localhost']
  };
  const result = outgoingMessage._renderHeaders();
  assert.deepStrictEqual(result, {
    host: 'nodejs.org',
    Origin: 'localhost'
  });
}
