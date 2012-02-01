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

var common = require('../common');
var assert = require('assert');
var http = require('http');

var outstanding_reqs = 0;
var test_req_body = 'some stuff...\n';
var test_res_body = 'other stuff!\n';
var sent_continue = false;
var got_continue = false;

function handler(req, res) {
  assert.equal(sent_continue, true, 'Full response sent before 100 Continue');
  common.debug('Server sending full response...');
  res.writeHead(200, {
    'Content-Type' : 'text/plain',
    'ABCD' : '1'
  });
  res.end(test_res_body);
}

var server = http.createServer(handler);
server.on('checkContinue', function(req, res) {
  common.debug('Server got Expect: 100-continue...');
  res.writeContinue();
  sent_continue = true;
  setTimeout(function() {
    handler(req, res);
  }, 100);
});
server.listen(common.PORT);



server.on('listening', function() {
  var req = http.request({
    port: common.PORT,
    method: 'POST',
    path: '/world',
    headers: { 'Expect': '100-continue' }
  });
  common.debug('Client sending request...');
  outstanding_reqs++;
  var body = '';
  req.on('continue', function() {
    common.debug('Client got 100 Continue...');
    got_continue = true;
    req.end(test_req_body);
  });
  req.on('response', function(res) {
    assert.equal(got_continue, true,
                 'Full response received before 100 Continue');
    assert.equal(200, res.statusCode,
                 'Final status code was ' + res.statusCode + ', not 200.');
    res.setEncoding('utf8');
    res.on('data', function(chunk) { body += chunk; });
    res.on('end', function() {
      common.debug('Got full response.');
      assert.equal(body, test_res_body, 'Response body doesn\'t match.');
      assert.ok('abcd' in res.headers, 'Response headers missing.');
      outstanding_reqs--;
      if (outstanding_reqs == 0) {
        server.close();
        process.exit();
      }
    });
  });
});
