'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }

// Check for:
// Spaced headers
// Psuedo headers
// Capitalized headers

const http2 = require('http2');
const { throws, strictEqual } = require('assert');

const server = http2.createServer(common.mustCall((req, res) => {
  throws(() => {
    res.setHeader(':path', '/');
  }, TypeError);
  throws(() => {
    res.setHeader('t est', 123);
  }, TypeError);
  res.setHeader('TEST', 123);
  res.setHeader('test_', 123);
  res.setHeader(' test', 123);
  res.end();
}));

server.listen(0, common.mustCall(() => {
  const session1 = http2.connect(`http://localhost:${server.address().port}`);
  session1.request({ 'test_': 123, 'TEST': 123 })
    .on('end', common.mustCall(() => {
      session1.close();
      server.close();
    }));
  const session2 = http2.connect(`http://localhost:${server.address().port}`);
  session2.on('error', common.mustCall((e) => {
    strictEqual(e.code, 'ERR_INVALID_HTTP_TOKEN');
  }));
  try {
    session2.request({ 't est': 123 });
  } catch (e) { } // eslint-disable-line no-unused-vars
  const session3 = http2.connect(`http://localhost:${server.address().port}`);
  session3.on('error', common.mustCall((e) => {
    strictEqual(e.code, 'ERR_INVALID_HTTP_TOKEN');
  }));
  try {
    session3.request({ ' test': 123 });
  } catch (e) { } // eslint-disable-line no-unused-vars
  const session4 = http2.connect(`http://localhost:${server.address().port}`);
  try {
    session4.request({ ':test': 123 });
  } catch (e) {
    strictEqual(e.code, 'ERR_HTTP2_INVALID_PSEUDOHEADER');
    session4.destroy();
  }
}));
