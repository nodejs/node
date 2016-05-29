'use strict';
require('../common');
var assert = require('assert');

var http = require('http');

var expect = 'hex\nutf8\n';
var data = '';
var ended = false;

process.on('exit', function() {
  assert(ended);
  assert.equal(data, expect);
  console.log('ok');
});

http.createServer(function(q, s) {
  s.setHeader('content-length', expect.length);
  s.write('6865780a', 'hex');
  s.write('utf8\n');
  s.end();
  this.close();
}).listen(0, function() {
  http.request({ port: this.address().port }).on('response', function(res) {
    res.setEncoding('ascii');
    res.on('data', function(c) {
      data += c;
    });
    res.on('end', function() {
      ended = true;
    });
  }).end();
});
