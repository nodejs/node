'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');

var bufferSize = 5 * 1024 * 1024;
var measuredSize = 0;

var buffer = Buffer.allocUnsafe(bufferSize);
for (var i = 0; i < buffer.length; i++) {
  buffer[i] = i % 256;
}


var web = http.Server(function(req, res) {
  web.close();

  console.log(req.headers);

  var i = 0;

  req.on('data', function(d) {
    process.stdout.write(',');
    measuredSize += d.length;
    for (var j = 0; j < d.length; j++) {
      assert.equal(buffer[i], d[j]);
      i++;
    }
  });


  req.on('end', function() {
    res.writeHead(200);
    res.write('thanks');
    res.end();
    console.log('response with \'thanks\'');
  });

  req.connection.on('error', function(e) {
    console.log('http server-side error: ' + e.message);
    process.exit(1);
  });
});

web.listen(0, common.mustCall(function() {
  console.log('Making request');

  var req = http.request({
    port: this.address().port,
    method: 'GET',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, common.mustCall(function(res) {
    console.log('Got response');
    res.setEncoding('utf8');
    res.on('data', common.mustCall(function(string) {
      assert.equal('thanks', string);
    }));
  }));
  req.end(buffer);
}));


process.on('exit', function() {
  assert.equal(bufferSize, measuredSize);
});
