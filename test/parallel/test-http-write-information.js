'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');

// Happy flow: arbitrary 1xx status with custom headers, observed by the
// client's 'information' event.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.writeInformation(110, { 'X-Progress': '50%', 'X-Stage': 'reading' });
    res.writeInformation(199, [['X-Custom', 'one'], ['X-Custom-2', 'two']]);
    res.end('done');
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({ port: server.address().port });

    const seen = [];
    req.on('information', (res) => {
      seen.push({
        statusCode: res.statusCode,
        headers: res.headers,
      });
    });

    req.on('response', common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);

      assert.strictEqual(seen.length, 2);
      assert.strictEqual(seen[0].statusCode, 110);
      assert.strictEqual(seen[0].headers['x-progress'], '50%');
      assert.strictEqual(seen[0].headers['x-stage'], 'reading');
      assert.strictEqual(seen[1].statusCode, 199);
      assert.strictEqual(seen[1].headers['x-custom'], 'one');
      assert.strictEqual(seen[1].headers['x-custom-2'], 'two');

      res.resume();
      res.on('end', common.mustCall(() => server.close()));
    }));

    req.end();
  }));
}

// Headers argument is optional / nullable.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.writeInformation(150);
    res.writeInformation(151, null);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({ port: server.address().port });
    let count = 0;
    req.on('information', () => count++);
    req.on('response', common.mustCall((res) => {
      assert.strictEqual(count, 2);
      res.resume();
      res.on('end', common.mustCall(() => server.close()));
    }));
    req.end();
  }));
}

// Error cases.
{
  const server = http.createServer(common.mustCall((req, res) => {
    assert.throws(() => res.writeInformation(101),
                  { code: 'ERR_HTTP_INVALID_STATUS_CODE' });
    assert.throws(() => res.writeInformation(99),
                  { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => res.writeInformation(200),
                  { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => res.writeInformation('100'),
                  { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => res.writeInformation(150, { 'X-Bad\n': 'v' }),
                  { code: 'ERR_INVALID_HTTP_TOKEN' });

    res.writeHead(200);
    assert.throws(() => res.writeInformation(150),
                  { code: 'ERR_HTTP_HEADERS_SENT' });
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({ port: server.address().port });
    req.on('response', common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => server.close()));
    }));
    req.end();
  }));
}
