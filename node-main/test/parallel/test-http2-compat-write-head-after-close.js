'use strict';

const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); };
const h2 = require('http2');

const server = h2.createServer((req, res) => {
  const stream = req.stream;
  stream.close();
  res.writeHead(200, { 'content-type': 'text/plain' });
});

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = h2.connect(`http://localhost:${port}`);
  const req = client.request({ ':path': '/' });
  req.on('response', common.mustNotCall('head after close should not be sent'));
  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.end();
}));
