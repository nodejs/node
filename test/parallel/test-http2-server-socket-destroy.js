// Flags: --expose-http2
'use strict';

const common = require('../common');
const h2 = require('http2');
const assert = require('assert');

const {
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_METHOD_POST
} = h2.constants;

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream) {
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.write('test');

  const socket = stream.session.socket;

  // When the socket is destroyed, the close events must be triggered
  // on the socket, server and session.
  socket.on('close', common.mustCall());
  server.on('close', common.mustCall());
  stream.session.on('close', common.mustCall(() => server.close()));

  // Also, the aborted event must be triggered on the stream
  stream.on('aborted', common.mustCall());

  assert.notStrictEqual(stream.session, undefined);
  socket.destroy();
  assert.strictEqual(stream.session, undefined);
}

server.listen(0);

server.on('listening', common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({
    [HTTP2_HEADER_PATH]: '/',
    [HTTP2_HEADER_METHOD]: HTTP2_METHOD_POST });

  req.on('aborted', common.mustCall());
  req.on('end', common.mustCall());
  req.on('response', common.mustCall());
  req.on('data', common.mustCall());

  client.on('close', common.mustCall());
}));
