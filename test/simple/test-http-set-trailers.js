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
server.listen(common.PORT);


// first, we test an HTTP/1.0 request.
server.addListener('listening', function() {
  var c = net.createConnection(common.PORT);
  var res_buffer = '';

  c.setEncoding('utf8');

  c.addListener('connect', function() {
    outstanding_reqs++;
    c.write('GET / HTTP/1.0\r\n\r\n');
  });

  c.addListener('data', function(chunk) {
    //console.log(chunk);
    res_buffer += chunk;
  });

  c.addListener('end', function() {
    c.end();
    assert.ok(! /x-foo/.test(res_buffer), 'Trailer in HTTP/1.0 response.');
    outstanding_reqs--;
    if (outstanding_reqs == 0) {
      server.close();
      process.exit();
    }
  });
});

// now, we test an HTTP/1.1 request.
server.addListener('listening', function() {
  var c = net.createConnection(common.PORT);
  var res_buffer = '';
  var tid;

  c.setEncoding('utf8');

  c.addListener('connect', function() {
    outstanding_reqs++;
    c.write('GET / HTTP/1.1\r\n\r\n');
    tid = setTimeout(assert.fail, 2000, 'Couldn\'t find last chunk.');
  });

  c.addListener('data', function(chunk) {
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
server.addListener('listening', function() {
  var client = http.createClient(common.PORT);
  var req = client.request('/hello', {});
  req.end();
  outstanding_reqs++;
  req.addListener('response', function(res) {
    res.addListener('end', function() {
      //console.log(res.trailers);
      assert.ok('x-foo' in res.trailers, 'Client doesn\'t see trailers.');
      outstanding_reqs--;
      if (outstanding_reqs == 0) {
        server.close();
        process.exit();
      }
    });
  });
});
