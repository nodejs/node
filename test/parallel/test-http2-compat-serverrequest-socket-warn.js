// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

// Http2ServerRequest.socket should warn one time when being accessed

const unsupportedWarned = common.mustCall(1);
process.on('warning', ({ name, message }) => {
  const expectedMessage =
    'Because the of the specific serialization and processing requirements ' +
    'imposed by the HTTP/2 protocol, it is not recommended for user code ' +
    'to read data from or write data to a Socket instance.';
  if (name === 'UnsupportedWarning' && message === expectedMessage)
    unsupportedWarned();
});

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    request.socket;
    request.socket; // should not warn
    response.socket; // should not warn
    response.end();
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers);
    request.on('end', common.mustCall(function() {
      client.destroy();
      server.close();
    }));
    request.end();
    request.resume();
  }));
}));
