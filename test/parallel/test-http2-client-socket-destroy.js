// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');
const body =
  '<html><head></head><body><h1>this is some data</h2></body></html>';

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream) {
  // The stream aborted event must have been triggered
  stream.on('aborted', common.mustCall());

  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.write(body);
}

server.listen(0);

server.on('listening', common.mustCall(function() {
  const client = h2.connect(`http://localhost:${this.address().port}`);

  const req = client.request({ ':path': '/' });

  req.on('response', common.mustCall(() => {
    // send a premature socket close
    client.socket.destroy();
  }));
  req.on('data', common.mustNotCall());

  req.on('end', common.mustCall(() => {
    server.close();
  }));

  // On the client, the close event must call
  client.on('close', common.mustCall());
  req.end();

}));
