'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer(common.mustCall(function(req, res) {
  res.end([`This will throw and destroy on session.request,
  but will not forward error on res.`]);
}));

server.listen(0, common.mustCall(function() {
  const session = http2.connect(`http://localhost:${server.address().port}`);
  const req = session.request();
  req.on('error', common.mustCall((e) => {
    assert.strictEqual(e.code, 'ERR_HTTP2_STREAM_ERROR');
    session.close();
    server.close();
  }));
}));
