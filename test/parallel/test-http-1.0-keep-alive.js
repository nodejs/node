'use strict';
require('../common');
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
  var server;
  if (test) {
    server = http.createServer(serverHandler).listen(0, '127.0.0.1', client);
  }
  var current = 0;

  function next() {
    check(tests.slice(1));
  }

  function serverHandler(req, res) {
    if (current + 1 === test.responses.length) this.close();
    var ctx = test.responses[current];
    console.error('<  SERVER SENDING RESPONSE', ctx);
    res.writeHead(200, ctx.headers);
    ctx.chunks.slice(0, -1).forEach(function(chunk) { res.write(chunk); });
    res.end(ctx.chunks[ctx.chunks.length - 1]);
  }

  function client() {
    if (current === test.requests.length) return next();
    const port = server.address().port;
    var conn = net.createConnection(port, '127.0.0.1', connected);

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
        conn.removeListener('data', ondata);
        connected();
      }
      conn.on('data', ondata);
    }
  }
}
