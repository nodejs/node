'use strict';
require('../common');
var assert = require('assert');

var http = require('http');

var Stream = require('stream');

function getSrc() {
  // An old-style readable stream.
  // The Readable class prevents this behavior.
  var src = new Stream();

  // start out paused, just so we don't miss anything yet.
  var paused = false;
  src.pause = function() {
    paused = true;
  };
  src.resume = function() {
    paused = false;
  };

  var chunks = [ '', 'asdf', '', 'foo', '', 'bar', '' ];
  var interval = setInterval(function() {
    if (paused)
      return;

    var chunk = chunks.shift();
    if (chunk !== undefined) {
      src.emit('data', chunk);
    } else {
      src.emit('end');
      clearInterval(interval);
    }
  }, 1);

  return src;
}


var expect = 'asdffoobar';

var server = http.createServer(function(req, res) {
  var actual = '';
  req.setEncoding('utf8');
  req.on('data', function(c) {
    actual += c;
  });
  req.on('end', function() {
    assert.equal(actual, expect);
    getSrc().pipe(res);
  });
  server.close();
});

server.listen(0, function() {
  var req = http.request({ port: this.address().port, method: 'POST' });
  var actual = '';
  req.on('response', function(res) {
    res.setEncoding('utf8');
    res.on('data', function(c) {
      actual += c;
    });
    res.on('end', function() {
      assert.equal(actual, expect);
    });
  });
  getSrc().pipe(req);
});

process.on('exit', function(c) {
  if (!c) console.log('ok');
});
