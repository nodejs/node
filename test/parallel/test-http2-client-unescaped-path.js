'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustNotCall());

const count = 32;

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  for (let i = 0; i <= count; i += 1) {
    const path = `bad${String.fromCharCode(i)}path`;
    assert.throws(() => client.request({ ':path': path }), {
      code: 'ERR_UNESCAPED_CHARACTERS',
      name: 'TypeError',
      message: 'Request path contains unescaped characters'
    });
  }

  assert.throws(() => client.request({ ':path': 'bad\u0100path' }), {
    code: 'ERR_UNESCAPED_CHARACTERS',
    name: 'TypeError',
    message: 'Request path contains unescaped characters'
  });

  client.close();
  server.close();
}));
