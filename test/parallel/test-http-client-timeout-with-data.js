'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var ntimeouts = 0;
var nchunks = 0;

process.on('exit', function() {
  assert.equal(ntimeouts, 1);
  assert.equal(nchunks, 2);
});

var options = {
  method: 'GET',
  port: common.PORT,
  host: '127.0.0.1',
  path: '/'
};

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length':'2'});
  res.write('*');
  setTimeout(function() { res.end('*'); }, 100);
});

server.listen(options.port, options.host, function() {
  var req = http.request(options, onresponse);
  req.end();

  function onresponse(res) {
    req.setTimeout(50, function() {
      assert.equal(nchunks, 1); // should have received the first chunk by now
      ntimeouts++;
    });

    res.on('data', function(data) {
      assert.equal('' + data, '*');
      nchunks++;
    });

    res.on('end', function() {
      assert.equal(nchunks, 2);
      server.close();
    });
  }
});
