'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

class MyServerResponse extends h2.Http2ServerResponse {
  status(code) {
    return this.writeHead(code, { 'Content-Type': 'text/plain' });
  }
}

const server = h2.createServer({
  Http2ServerResponse: MyServerResponse
}, (req, res) => {
  res.status(200);
  res.end();
});
server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':path': '/' });

  req.on('response', common.mustCall());

  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
}));
