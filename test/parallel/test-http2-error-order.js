'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createServer, connect } = require('http2');

const messages = [];
const expected = [
  'Stream:created',
  'Stream:error',
  'Stream:close',
  'Request:error',
];

const server = createServer();

server.on('stream', (stream) => {
  messages.push('Stream:created');
  stream
    .on('close', () => messages.push('Stream:close'))
    .on('error', (err) => messages.push('Stream:error'))
    .respondWithFile('dont exist');
});

server.listen(0);

const client = connect(`http://localhost:${server.address().port}`);
const req = client.request();

req.on('response', common.mustNotCall());

req.on('error', () => {
  messages.push('Request:error');
  client.close();
});

client.on('close', common.mustCall(() => {
  assert.deepStrictEqual(messages, expected);
  server.close();
}));
