'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const fixtures = require('../common/fixtures');

const fname = fixtures.path('elipses.txt');

const server = http2.createServer(common.mustCall((request, response) => {
  response.stream.respondWithFile(fname);
}));
server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.on('response', common.mustCall());
  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.end();
  req.resume();
});
