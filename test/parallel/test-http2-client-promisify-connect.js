'use strict';

const common = require('../common');
common.crashOnUnhandledRejection();

if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const util = require('util');

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end('ok');
}));
server.listen(0, common.mustCall(() => {
  const connect = util.promisify(http2.connect);

  connect(`http://localhost:${server.address().port}`)
    .catch(common.mustNotCall())
    .then(common.mustCall((client) => {
      assert(client);
      const req = client.request();
      let data = '';
      req.setEncoding('utf8');
      req.on('data', (chunk) => data += chunk);
      req.on('end', common.mustCall(() => {
        assert.strictEqual(data, 'ok');
        client.close();
        server.close();
      }));
    }));
}));
