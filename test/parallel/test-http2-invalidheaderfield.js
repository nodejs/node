// spaced headers
// psuedo heaers
// capitalized headers

'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const http2 = require('http2');
const { throws } = require('assert');

const server = http2.createServer(common.mustCall((req, res) => {

  throws(() => {
    res.setHeader(':path', '/');
  }, TypeError);

  throws(() => {
    res.setHeader('t est', 123);
  }, TypeError);

  // should not fail
  res.setHeader('TEST', 123);

  // should not fail
  res.setHeader('test_', 123);

  res.end();
}, 2));

server.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server.address().port}`);

  throws(() => {
    session.request({
      't est': 123
    });
  }, TypeError);

  throws(() => {
    session.request({
      ':test': 123
    });
  }, TypeError);

  // should not fail
  session.request({
    'TEST': 123
  });

  // should not fail
  const req = session.request({
    'test_': 123
  });

  req.on('end', () => {
    session.close();
    server.close();
  });
}));
