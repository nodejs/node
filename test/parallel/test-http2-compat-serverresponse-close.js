'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

// Server request and response should receive close event
// if the connection was terminated before response.end
// could be called or flushed

const server = h2.createServer(common.mustCall((req, res) => {
  res.writeHead(200);
  res.write('a');

  req.on('close', common.mustCall());
  res.on('close', common.mustCall());
  req.on('error', common.mustNotCall());
}));
server.listen(0);

server.on('listening', () => {
  const url = `http://localhost:${server.address().port}`;
  const client = h2.connect(url, common.mustCall(() => {
    const request = client.request();
    request.on('data', common.mustCall(function(chunk) {
      client.destroy();
      server.close();
    }));
  }));
});
