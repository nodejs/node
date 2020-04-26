'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const h2 = require('http2');
const assert = require('assert');

const server = h2.createServer(common.mustCall((req, res) => {
  assert.throws(() => {
    res.setHeader('foo⠊Set-Cookie', 'foo=bar');
  }, { code: 'ERR_INVALID_HTTP_TOKEN' });
  assert.throws(() => {
    res.writeHead(200, { 'foo⠊Set-Cookie': 'foo=bar' });
  }, { code: 'ERR_INVALID_HTTP_TOKEN' });
  assert.throws(() => {
    res.setHeader('Set-Cookie', 'foo=barഊഊ<script>alert("Hi!")</script>');
  }, { code: 'ERR_INVALID_CHAR' });
  assert.throws(() => {
    res.writeHead(200, {
      'Set-Cookie': 'foo=barഊഊ<script>alert("Hi!")</script>'
    });
  }, { code: 'ERR_INVALID_CHAR' });
  res.end();
}));

server.listen(0, common.mustCall(() => {
  const session = h2.connect(`http://localhost:${server.address().port}`);
  const req = session.request();
  req.on('end', () => {
    session.close();
    server.close();
  });
}));
