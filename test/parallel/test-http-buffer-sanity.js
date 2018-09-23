'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var util = require('util');

var bufferSize = 5 * 1024 * 1024;
var measuredSize = 0;

var buffer = Buffer(bufferSize);
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

var gotThanks = false;

web.listen(common.PORT, function() {
  console.log('Making request');

  var req = http.request({
    port: common.PORT,
    method: 'GET',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, function(res) {
    console.log('Got response');
    res.setEncoding('utf8');
    res.on('data', function(string) {
      assert.equal('thanks', string);
      gotThanks = true;
    });
  });
  req.end(buffer);
});


process.on('exit', function() {
  assert.equal(bufferSize, measuredSize);
  assert.ok(gotThanks);
});
