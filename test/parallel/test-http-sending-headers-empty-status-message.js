'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

for (const initialMessage of [undefined, '', 'Before']) {
  for (const method of ['writeHead', 'write', 'end', 'flushHeaders']) {
    const server = http.createServer(common.mustCall((req, res) => {
      res.statusMessage = initialMessage;
      res.once('sendingHeaders', common.mustCall(function() {
        this.statusCode = 503;
        this.statusMessage = '';
      }));

      switch (method) {
        case 'writeHead':
          res.writeHead(200);
          break;
        case 'write':
          res.write('body');
          break;
        case 'flushHeaders':
          res.flushHeaders();
          break;
      }
      res.end();
      assert.strictEqual(res.statusMessage, '');
    }));

    server.listen(0, common.mustCall(() => {
      http.get({ port: server.address().port }, common.mustCall((res) => {
        assert.strictEqual(res.statusCode, 503);
        assert.strictEqual(res.statusMessage, '');
        res.resume().on('end', common.mustCall(() => server.close()));
      }));
    }));
  }
}

// An unchanged implicit reason still defaults to the final status code.
for (const initialMessage of [undefined, null, false, 0, '']) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.statusMessage = initialMessage;
    res.once('sendingHeaders', common.mustCall(function() {
      this.statusCode = 503;
    }));
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 503);
      assert.strictEqual(res.statusMessage, 'Service Unavailable');
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// The nested writeHead() must also preserve an empty reason chosen by the
// listener, while the outer call still rejects flushing from inside the hook.
{
  const res = new http.ServerResponse({
    method: 'GET', httpVersionMajor: 1, httpVersionMinor: 1,
  });
  res.once('sendingHeaders', common.mustCall(function() {
    this.statusMessage = '';
    this.flushHeaders();
  }));
  assert.throws(() => res.writeHead(200), { code: 'ERR_HTTP_HEADERS_SENT' });
  assert.strictEqual(res.statusMessage, '');
}
