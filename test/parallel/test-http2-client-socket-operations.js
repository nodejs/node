'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':path': '/' });

  req.on(
    'response',
    common.mustCall(() => {
      client.socket.pause();
      // Force 'pause' when _handle.reading is false
      client.socket.emit('pause');

      client.socket.resume();
      // Force 'resume' when _handle.reading is true
      client.socket.emit('resume');

      client.socket.emit('drain');
    })
  );
  req.on('data', common.mustNotCall());

  req.on(
    'end',
    common.mustCall(() => {
      server.close();
      client.destroy();
    })
  );

  // On the client, the close event must call
  client.on('close', common.mustCall());
  req.end();
}));
