'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { PerformanceObserver } = require('perf_hooks');

const http2Server = http2.createServer(common.mustCall(function(req, res) {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(Buffer.from('9'.repeat(1024)), () => {
    http2Server.close();
  });
}));

http2Server.listen(0, common.mustCall(function() {
  const URL = `http://localhost:${http2Server.address().port}`;
  const http2client = http2.connect(URL, { protocol: 'http:' });
  const req = http2client.request({ ':method': 'GET', ':path': '/' });
  req.on('response', common.mustCall());
  req.on('data', common.mustCall());
  req.on('end', common.mustCall(function() {
    http2client.close();
    observer.disconnect();
  }));
  req.end();
}));

const observer = new PerformanceObserver(common.mustCall(function(items) {
  const entry = items.getEntries()[0];
  if (entry.name === 'Http2Stream') {
    assert.strictEqual(entry.bytesWritten, 1024);
  }
}));

observer.observe({ entryTypes: ['http2'] });
