'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const test_res_body = 'other stuff!\n';
const countdown = new Countdown(2, () => server.close());

const server = http.createServer((req, res) => {
  console.error('Server sending informational message #1...');
  // These function calls may rewritten as necessary
  // to call res.writeHead instead
  res._writeRaw('HTTP/1.1 102 Processing\r\n');
  res._writeRaw('Foo: Bar\r\n');
  res._writeRaw('\r\n');
  console.error('Server sending full response...');
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'ABCD': '1'
  });
  res.end(test_res_body);
});

server.listen(0, function() {
  const req = http.request({
    port: this.address().port,
    path: '/world'
  });
  req.end();
  console.error('Client sending request...');

  let body = '';

  req.on('information', function(res) {
    assert.strictEqual(res.httpVersion, '1.1');
    assert.strictEqual(res.httpVersionMajor, 1);
    assert.strictEqual(res.httpVersionMinor, 1);
    assert.strictEqual(res.statusCode, 102,
                       `Received ${res.statusCode}, not 102.`);
    assert.strictEqual(res.statusMessage, 'Processing',
                       `Received ${res.statusMessage}, not "Processing".`);
    assert.strictEqual(res.headers.foo, 'Bar');
    assert.strictEqual(res.rawHeaders[0], 'Foo');
    assert.strictEqual(res.rawHeaders[1], 'Bar');
    console.error('Client got 102 Processing...');
    countdown.dec();
  });

  req.on('response', function(res) {
    // Check that all 102 Processing received before full response received.
    assert.strictEqual(countdown.remaining, 1);
    assert.strictEqual(res.statusCode, 200,
                       `Final status code was ${res.statusCode}, not 200.`);
    res.setEncoding('utf8');
    res.on('data', function(chunk) { body += chunk; });
    res.on('end', function() {
      console.error('Got full response.');
      assert.strictEqual(body, test_res_body);
      assert.ok('abcd' in res.headers);
      countdown.dec();
    });
  });
});
