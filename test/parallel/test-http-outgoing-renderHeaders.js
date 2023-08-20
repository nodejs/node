'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');

const kOutHeaders = require('internal/http').kOutHeaders;
const http = require('http');
const OutgoingMessage = http.OutgoingMessage;

{
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage._header = {};
  assert.throws(
    () => outgoingMessage._renderHeaders(),
    {
      code: 'ERR_HTTP_HEADERS_SENT',
      name: 'Error',
      message: 'Cannot render headers after they are sent to the client'
    }
  );
}

{
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage[kOutHeaders] = null;
  const result = outgoingMessage._renderHeaders();
  assert.deepStrictEqual(result, {});
}


{
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage[kOutHeaders] = {};
  const result = outgoingMessage._renderHeaders();
  assert.deepStrictEqual(result, {});
}

{
  const outgoingMessage = new OutgoingMessage();
  outgoingMessage[kOutHeaders] = {
    host: ['host', 'nodejs.org'],
    origin: ['Origin', 'localhost']
  };
  const result = outgoingMessage._renderHeaders();
  assert.deepStrictEqual(result, {
    host: 'nodejs.org',
    Origin: 'localhost'
  });
}
