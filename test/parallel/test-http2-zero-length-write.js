// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const { Readable } = require('stream');

function getSrc() {
  const chunks = [ '', 'asdf', '', 'foo', '', 'bar', '' ];
  return new Readable({
    read() {
      const chunk = chunks.shift();
      if (chunk !== undefined)
        this.push(chunk);
      else
        this.push(null);
    }
  });
}

const expect = 'asdffoobar';

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  let actual = '';
  stream.respond();
  stream.resume();
  stream.setEncoding('utf8');
  stream.on('data', (chunk) => actual += chunk);
  stream.on('end', common.mustCall(() => {
    getSrc().pipe(stream);
    assert.strictEqual(actual, expect);
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  let actual = '';
  const req = client.request({ ':method': 'POST' });
  req.on('response', common.mustCall());
  req.on('data', (chunk) => actual += chunk);
  req.on('end', common.mustCall(() => {
    assert.strictEqual(actual, expect);
    server.close();
    client.destroy();
  }));
  getSrc().pipe(req);
}));
