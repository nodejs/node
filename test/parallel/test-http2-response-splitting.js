'use strict';

// Response splitting is no longer an issue with HTTP/2. The underlying
// nghttp2 implementation automatically strips out the header values that
// contain invalid characters.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { URL } = require('url');

// Response splitting example, credit: Amit Klein, Safebreach
const str = '/welcome?lang=bar%c4%8d%c4%8aContent­Length:%200%c4%8d%c4%8a%c' +
            '4%8d%c4%8aHTTP/1.1%20200%20OK%c4%8d%c4%8aContent­Length:%202' +
            '0%c4%8d%c4%8aLast­Modified:%20Mon,%2027%20Oct%202003%2014:50:18' +
            '%20GMT%c4%8d%c4%8aContent­Type:%20text/html%c4%8d%c4%8a%c4%8' +
            'd%c4%8a%3chtml%3eGotcha!%3c/html%3e';

// Response splitting example, credit: Сковорода Никита Андреевич (@ChALkeR)
const x = 'fooഊSet-Cookie: foo=barഊഊ<script>alert("Hi!")</script>';
const y = 'foo⠊Set-Cookie: foo=bar';

let remaining = 3;

function makeUrl(headers) {
  return `${headers[':scheme']}://${headers[':authority']}${headers[':path']}`;
}

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {

  const obj = Object.create(null);
  switch (remaining--) {
    case 3:
      const url = new URL(makeUrl(headers));
      obj[':status'] = 302;
      obj.Location = `/foo?lang=${url.searchParams.get('lang')}`;
      break;
    case 2:
      obj.foo = x;
      break;
    case 1:
      obj.foo = y;
      break;
  }
  stream.respond(obj);
  stream.end();
}, 3));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  function maybeClose() {
    if (remaining === 0) {
      server.close();
      client.close();
    }
  }

  function doTest(path, key, expected) {
    const req = client.request({ ':path': path });
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers.foo, undefined);
      assert.strictEqual(headers.location, undefined);
    }));
    req.resume();
    req.on('end', common.mustCall());
    req.on('close', common.mustCall(maybeClose));
  }

  doTest(str, 'location', str);
  doTest('/', 'foo', x);
  doTest('/', 'foo', y);

}));
