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
var net = require('net');

// Check that our HTTP server correctly handles HTTP/1.0 keep-alive requests.
check([{
  name: 'keep-alive, no TE header',
  requests: [{
    expectClose: true,
    data: 'POST / HTTP/1.0\r\n' +
          'Connection: keep-alive\r\n' +
          '\r\n'
  }, {
    expectClose: true,
    data: 'POST / HTTP/1.0\r\n' +
          'Connection: keep-alive\r\n' +
          '\r\n'
  }],
  responses: [{
    headers: {'Connection': 'keep-alive'},
    chunks: ['OK']
  }, {
    chunks: []
  }]
}, {
  name: 'keep-alive, with TE: chunked',
  requests: [{
    expectClose: false,
    data: 'POST / HTTP/1.0\r\n' +
          'Connection: keep-alive\r\n' +
          'TE: chunked\r\n' +
          '\r\n'
  }, {
    expectClose: true,
    data: 'POST / HTTP/1.0\r\n' +
          '\r\n'
  }],
  responses: [{
    headers: {'Connection': 'keep-alive'},
    chunks: ['OK']
  }, {
    chunks: []
  }]
}, {
  name: 'keep-alive, with Transfer-Encoding: chunked',
  requests: [{
    expectClose: false,
    data: 'POST / HTTP/1.0\r\n' +
          'Connection: keep-alive\r\n' +
          '\r\n'
  }, {
    expectClose: true,
    data: 'POST / HTTP/1.0\r\n' +
          '\r\n'
  }],
  responses: [{
    headers: {'Connection': 'keep-alive',
              'Transfer-Encoding': 'chunked'},
    chunks: ['OK']
  }, {
    chunks: []
  }]
}, {
  name: 'keep-alive, with Content-Length',
  requests: [{
    expectClose: false,
    data: 'POST / HTTP/1.0\r\n' +
          'Connection: keep-alive\r\n' +
          '\r\n'
  }, {
    expectClose: true,
    data: 'POST / HTTP/1.0\r\n' +
          '\r\n'
  }],
  responses: [{
    headers: {'Connection': 'keep-alive',
              'Content-Length': '2'},
    chunks: ['OK']
  }, {
    chunks: []
  }]
}]);

function check(tests) {
  var test = tests[0];
  if (test) http.createServer(server).listen(common.PORT, '127.0.0.1', client);
  var current = 0;

  function next() {
    check(tests.slice(1));
  }

  function server(req, res) {
    if (current + 1 === test.responses.length) this.close();
    var ctx = test.responses[current];
    console.error('<  SERVER SENDING RESPONSE', ctx);
    res.writeHead(200, ctx.headers);
    ctx.chunks.slice(0, -1).forEach(function(chunk) { res.write(chunk) });
    res.end(ctx.chunks[ctx.chunks.length - 1]);
  }

  function client() {
    if (current === test.requests.length) return next();
    var conn = net.createConnection(common.PORT, '127.0.0.1', connected);

    function connected() {
      var ctx = test.requests[current];
      console.error(' > CLIENT SENDING REQUEST', ctx);
      conn.setEncoding('utf8');
      conn.write(ctx.data);

      function onclose() {
        console.error(' > CLIENT CLOSE');
        if (!ctx.expectClose) throw new Error('unexpected close');
        client();
      }
      conn.on('close', onclose);

      function ondata(s) {
        console.error(' > CLIENT ONDATA %j %j', s.length, s.toString());
        current++;
        if (ctx.expectClose) return;
        conn.removeListener('close', onclose);
        conn.removeListener('data', ondata);;
        connected();
      }
      conn.on('data', ondata);
    }
  }
}
