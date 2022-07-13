'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

{
  const server = http2.createServer((req, res) => {
    req.pipe(res);
  });

  server.listen(0, () => {
    const url = `http://localhost:${server.address().port}`;
    const client = http2.connect(url);
    const req = client.request({ ':method': 'POST' });

    for (let i = 0; i < 4000; i++) {
      req.write(Buffer.alloc(6));
    }

    req.on('close', common.mustCall(() => {
      console.log('(req onclose)');
      server.close();
      client.close();
    }));

    req.once('data', common.mustCall(() => req.destroy()));
  });
}
