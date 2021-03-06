// Flags: --expose-internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const { kSocket } = require('internal/http2/util');

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream) {
  stream.respond();
  stream.write('test');

  const socket = stream.session[kSocket];

  // When the socket is destroyed, the close events must be triggered
  // on the socket, server and session.
  socket.on('close', common.mustCall());
  stream.on('close', common.mustCall());
  server.on('close', common.mustCall());
  stream.session.on('close', common.mustCall(() => server.close()));

  // Also, the aborted event must be triggered on the stream
  stream.on('aborted', common.mustCall());

  assert.notStrictEqual(stream.session, undefined);
  socket.destroy();
}

server.listen(0);

server.on('listening', common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  // The client may have an ECONNRESET error here depending on the operating
  // system, due mainly to differences in the timing of socket closing. Do
  // not wrap this in a common mustCall.
  client.on('error', (err) => {
    if (err.code !== 'ECONNRESET')
      throw err;
  });
  client.on('close', common.mustCall());

  const req = client.request({ ':method': 'POST' });
  // The client may have an ECONNRESET error here depending on the operating
  // system, due mainly to differences in the timing of socket closing. Do
  // not wrap this in a common mustCall.
  req.on('error', (err) => {
    if (err.code !== 'ECONNRESET')
      throw err;
  });

  req.on('aborted', common.mustCall());
  req.resume();
  req.on('end', common.mustCall());
}));
