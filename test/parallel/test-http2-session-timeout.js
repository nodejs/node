// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const serverTimeout = common.platformTimeout(200);
const callTimeout = common.platformTimeout(10);

const server = h2.createServer();
server.timeout = serverTimeout;

server.on('request', (req, res) => res.end());
server.on('timeout', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  const url = `http://localhost:${port}`;
  const client = h2.connect(url);
  makeReq(40);

  function makeReq(attempts) {
    const request = client.request({
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`,
    });
    request.end();

    if (attempts) {
      setTimeout(() => makeReq(attempts - 1), callTimeout);
    } else {
      server.close();
      client.destroy();
    }
  }
}));
