'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var outstanding_reqs = 0;

var server = http.createServer(function(req, res) {
  res.writeHead(200, [['content-type', 'text/plain']]);
  res.addTrailers({'x-foo': 'bar'});
  res.end('stuff' + '\n');
});
server.listen(0);


// first, we test an HTTP/1.0 request.
server.on('listening', function() {
  var c = net.createConnection(this.address().port);
  var res_buffer = '';

  c.setEncoding('utf8');

  c.on('connect', function() {
    outstanding_reqs++;
    c.write('GET / HTTP/1.0\r\n\r\n');
  });

  c.on('data', function(chunk) {
    //console.log(chunk);
    res_buffer += chunk;
  });

  c.on('end', function() {
    c.end();
    assert.ok(!/x-foo/.test(res_buffer), 'Trailer in HTTP/1.0 response.');
    outstanding_reqs--;
    if (outstanding_reqs == 0) {
      server.close();
      process.exit();
    }
  });
});

// now, we test an HTTP/1.1 request.
server.on('listening', function() {
  var c = net.createConnection(this.address().port);
  var res_buffer = '';
  var tid;

  c.setEncoding('utf8');

  c.on('connect', function() {
    outstanding_reqs++;
    c.write('GET / HTTP/1.1\r\n\r\n');
    tid = setTimeout(common.fail, 2000, 'Couldn\'t find last chunk.');
  });

  c.on('data', function(chunk) {
    //console.log(chunk);
    res_buffer += chunk;
    if (/0\r\n/.test(res_buffer)) { // got the end.
      outstanding_reqs--;
      clearTimeout(tid);
      assert.ok(
          /0\r\nx-foo: bar\r\n\r\n$/.test(res_buffer),
          'No trailer in HTTP/1.1 response.'
      );
      if (outstanding_reqs == 0) {
        server.close();
        process.exit();
      }
    }
  });
});

// now, see if the client sees the trailers.
server.on('listening', function() {
  http.get({
    port: this.address().port,
    path: '/hello',
    headers: {}
  }, function(res) {
    res.on('end', function() {
      //console.log(res.trailers);
      assert.ok('x-foo' in res.trailers, 'Client doesn\'t see trailers.');
      outstanding_reqs--;
      if (outstanding_reqs == 0) {
        server.close();
        process.exit();
      }
    });
    res.resume();
  });
  outstanding_reqs++;
});
