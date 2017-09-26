// Flags: --expose-http2 --expose-internals
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
}));
server.listen(0);

server.on('listening', function() {
  const port = server.address().port;

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`,
    };
    const request = client.request(headers);
    request.on('data', common.mustCall(function(chunk) {
      // cause an error on the server side
      client.destroy();
      server.close();
    }));
    request.end();
  }));
});
