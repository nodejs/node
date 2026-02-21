'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const msecs = common.platformTimeout(1);
const server = http2.createServer();

server.on('request', (req, res) => {
  res.setTimeout(msecs, common.mustCall(() => {
    res.end();
  }));
  res.on('timeout', common.mustCall());
  res.on('finish', common.mustCall(() => {
    res.setTimeout(msecs, common.mustNotCall());
    process.nextTick(() => {
      res.setTimeout(msecs, common.mustNotCall());
      server.close();
    });
  }));
});

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request({
    ':path': '/',
    ':method': 'GET',
    ':scheme': 'http',
    ':authority': `localhost:${port}`
  });
  req.on('end', common.mustCall(() => {
    client.close();
  }));
  req.resume();
  req.end();
}));
