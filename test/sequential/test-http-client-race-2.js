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
var url = require('url');

//
// Slight variation on test-http-client-race to test for another race
// condition involving the parsers FreeList used internally by http.Client.
//

var body1_s = '1111111111111111';
var body2_s = '22222';
var body3_s = '3333333333333333333';

var server = http.createServer(function(req, res) {
  var pathname = url.parse(req.url).pathname;

  var body;
  switch (pathname) {
    case '/1': body = body1_s; break;
    case '/2': body = body2_s; break;
    default: body = body3_s;
  }

  res.writeHead(200,
                {'Content-Type': 'text/plain', 'Content-Length': body.length});
  res.end(body);
});
server.listen(common.PORT);

var body1 = '';
var body2 = '';
var body3 = '';

server.on('listening', function() {
  var client = http.createClient(common.PORT);

  //
  // Client #1 is assigned Parser #1
  //
  var req1 = client.request('/1');
  req1.end();
  req1.on('response', function(res1) {
    res1.setEncoding('utf8');

    res1.on('data', function(chunk) {
      body1 += chunk;
    });

    res1.on('end', function() {
      //
      // Delay execution a little to allow the 'close' event to be processed
      // (required to trigger this bug!)
      //
      setTimeout(function() {
        //
        // The bug would introduce itself here: Client #2 would be allocated the
        // parser that previously belonged to Client #1. But we're not finished
        // with Client #1 yet!
        //
        var client2 = http.createClient(common.PORT);

        //
        // At this point, the bug would manifest itself and crash because the
        // internal state of the parser was no longer valid for use by Client #1
        //
        var req2 = client.request('/2');
        req2.end();
        req2.on('response', function(res2) {
          res2.setEncoding('utf8');
          res2.on('data', function(chunk) { body2 += chunk; });
          res2.on('end', function() {

            //
            // Just to be really sure we've covered all our bases, execute a
            // request using client2.
            //
            var req3 = client2.request('/3');
            req3.end();
            req3.on('response', function(res3) {
              res3.setEncoding('utf8');
              res3.on('data', function(chunk) { body3 += chunk });
              res3.on('end', function() { server.close(); });
            });
          });
        });
      }, 500);
    });
  });
});

process.on('exit', function() {
  assert.equal(body1_s, body1);
  assert.equal(body2_s, body2);
  assert.equal(body3_s, body3);
});

