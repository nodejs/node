// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const test_req_body = 'some stuff...\n';
const test_res_body = 'other stuff!\n';
let sent_continue = false;
let got_continue = false;

const handler = common.mustCall((req, res) => {
  assert.ok(sent_continue, 'Full response sent before 100 Continue');
  console.error('Server sending full response...');
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'ABCD': '1'
  });
  res.end(test_res_body);
});

const server = http.createServer(common.mustNotCall());
server.on('checkContinue', common.mustCall((req, res) => {
  console.error('Server got Expect: 100-continue...');
  res.writeContinue();
  sent_continue = true;
  setTimeout(function() {
    handler(req, res);
  }, 100);
}));
server.listen(0);


server.on('listening', common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    method: 'POST',
    path: '/world',
    headers: { 'Expect': '100-continue' }
  });
  console.error('Client sending request...');
  let body = '';
  req.on('continue', common.mustCall(() => {
    console.error('Client got 100 Continue...');
    got_continue = true;
    req.end(test_req_body);
  }));
  req.on('response', common.mustCall((res) => {
    assert.ok(got_continue, 'Full response received before 100 Continue');
    assert.strictEqual(res.statusCode, 200,
                       `Final status code was ${res.statusCode}, not 200.`);
    res.setEncoding('utf8');
    res.on('data', function(chunk) { body += chunk; });
    res.on('end', common.mustCall(() => {
      console.error('Got full response.');
      assert.strictEqual(body, test_res_body);
      assert.ok('abcd' in res.headers, 'Response headers missing.');
      server.close();
      process.exit();
    }));
  }));
}));
