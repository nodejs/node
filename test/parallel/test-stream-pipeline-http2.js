'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { Readable, pipeline } = require('stream');
const http2 = require('http2');

{
  const server = http2.createServer(common.mustCallAtLeast((req, res) => {
    pipeline(req, res, common.mustCall());
  }));

  server.listen(0, common.mustCall(() => {
    const url = `http://localhost:${server.address().port}`;
    const client = http2.connect(url);
    const req = client.request({ ':method': 'POST' });

    const rs = new Readable({
      read() {
        rs.push('hello');
      }
    });

    pipeline(rs, req, common.mustCall((err) => {
      server.close();
      client.close();
    }));

    let received = 0;
    req.on('data', (data) => {
      received += data.length;
      // Use a byte threshold because data event chunking varies by platform.
      if (received >= 32 * 1024) rs.destroy();
    });
  }));
}
