'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('node:assert');
const http2 = require('node:http2');
const debug = require('node:util').debuglog('test');

const testResBody = 'response content';

{
  // Happy flow - string argument

  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({
      link: '</styles.css>; rel=preload; as=style'
    });

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0);

  server.on('listening', common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    debug('Client sending request...');

    req.on('headers', common.mustCall((headers) => {
      assert.notStrictEqual(headers, undefined);
      assert.strictEqual(headers[':status'], 103);
      assert.strictEqual(headers.link, '</styles.css>; rel=preload; as=style');
    }));

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

{
  // Happy flow - array argument

  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({
      link: [
        '</styles.css>; rel=preload; as=style',
        '</scripts.js>; rel=preload; as=script',
      ]
    });

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0);

  server.on('listening', common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    debug('Client sending request...');

    req.on('headers', common.mustCall((headers) => {
      assert.notStrictEqual(headers, undefined);
      assert.strictEqual(headers[':status'], 103);
      assert.strictEqual(
        headers.link,
        '</styles.css>; rel=preload; as=style, </scripts.js>; rel=preload; as=script'
      );
    }));

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

{
  // Happy flow - empty array

  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints({
      link: []
    });

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
