'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const debug = require('util').debuglog('test');

const testResBody = 'other stuff!\n';
const kMessageCount = 2;

const server = http.createServer((req, res) => {
  for (let i = 0; i < kMessageCount; i++) {
    debug(`Server sending informational message #${i}...`);
    res.writeProcessing();
  }
  debug('Server sending full response...');
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'ABCD': '1'
  });
  res.end(testResBody);
});

server.listen(0, function() {
  const req = http.request({
    port: this.address().port,
    path: '/world'
  });
  req.end();
  debug('Client sending request...');

  let body = '';
  let infoCount = 0;

  req.on('information', () => { infoCount++; });

  req.on('response', function(res) {
    // Check that all 102 Processing received before full response received.
    assert.strictEqual(infoCount, kMessageCount);
    assert.strictEqual(res.statusCode, 200,
                       `Final status code was ${res.statusCode}, not 200.`);
    res.setEncoding('utf8');
    res.on('data', function(chunk) { body += chunk; });
    res.on('end', function() {
      debug('Got full response.');
      assert.strictEqual(body, testResBody);
      assert.ok('abcd' in res.headers);
      server.close();
    });
  });
});
