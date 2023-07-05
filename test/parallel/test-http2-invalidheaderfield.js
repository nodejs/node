'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }

// Check for:
// Spaced headers
// Pseudo headers
// Capitalized headers

const http2 = require('http2');
const { throws, strictEqual } = require('assert');

{
  const server = http2.createServer(common.mustCall((req, res) => {
    throws(() => {
      res.setHeader(':path', '/');
    }, {
      code: 'ERR_HTTP2_PSEUDOHEADER_NOT_ALLOWED'
    });
    throws(() => {
      res.setHeader('t est', 123);
    }, {
      code: 'ERR_INVALID_HTTP_TOKEN'
    });
    res.setHeader('TEST', 123);
    res.setHeader('test_', 123);
    res.setHeader(' test', 123);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const session = http2.connect(`http://localhost:${server.address().port}`);
    session.request({ 'test_': 123, 'TEST': 123 })
      .on('end', common.mustCall(() => {
        session.close();
        server.close();
      }));
  }));
}

{
  const server = http2.createServer();
  server.listen(0, common.mustCall(() => {
    const session = http2.connect(`http://localhost:${server.address().port}`);
    session.on('error', common.mustCall((e) => {
      strictEqual(e.code, 'ERR_INVALID_HTTP_TOKEN');
      server.close();
    }));
    throws(() => {
      session.request({ 't est': 123 });
    }, {
      code: 'ERR_INVALID_HTTP_TOKEN'
    });
  }));
}


{
  const server = http2.createServer();
  server.listen(0, common.mustCall(() => {
    const session = http2.connect(`http://localhost:${server.address().port}`);
    session.on('error', common.mustCall((e) => {
      strictEqual(e.code, 'ERR_INVALID_HTTP_TOKEN');
      server.close();
    }));
    throws(() => {
      session.request({ ' test': 123 });
    }, {
      code: 'ERR_INVALID_HTTP_TOKEN'
    });
  }));
}

{
  const server = http2.createServer();
  server.listen(0, common.mustCall(() => {
    const session4 = http2.connect(`http://localhost:${server.address().port}`);
    throws(() => {
      session4.request({ ':test': 123 });
    }, {
      code: 'ERR_HTTP2_INVALID_PSEUDOHEADER'
    });
    session4.close();
    server.close();
  }));
}
