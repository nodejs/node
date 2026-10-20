'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('node:assert');
const http2 = require('node:http2');
const debug = require('node:util').debuglog('test');

const testResBody = 'response content';

{
  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    debug('Server sending early hints...');

    assert.throws(() => {
      res.writeEarlyHints({
        'link': '</styles.css>; rel=preload; as=style',
        'x\rbad': 'value',
      });
    }, (err) => err.code === 'ERR_INVALID_HTTP_TOKEN');

    assert.throws(() => {
      res.writeEarlyHints({
        'link': '</styles.css>; rel=preload; as=style',
        'x-custom': undefined,
      });
    }, (err) => err.code === 'ERR_HTTP2_INVALID_HEADER_VALUE');

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0);

  server.on('listening', common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    debug('Client sending request...');

    req.on('headers', common.mustNotCall());

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
    }));

    let data = '';
    req.on('data', common.mustCallAtLeast((d) => data += d));

    req.on('end', common.mustCall(() => {
      debug('Got full response.');
      assert.strictEqual(data, testResBody);
      client.close();
      server.close();
    }));
  }));
}
