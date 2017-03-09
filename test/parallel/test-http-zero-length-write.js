'use strict';
require('../common');
const assert = require('assert');

const http = require('http');

const Stream = require('stream');

function getSrc() {
  // An old-style readable stream.
  // The Readable class prevents this behavior.
  const src = new Stream();

  // start out paused, just so we don't miss anything yet.
  let paused = false;
  src.pause = function() {
    paused = true;
  };
  src.resume = function() {
    paused = false;
  };

  const chunks = [ '', 'asdf', '', 'foo', '', 'bar', '' ];
  const interval = setInterval(function() {
    if (paused)
      return;

    const chunk = chunks.shift();
    if (chunk !== undefined) {
      src.emit('data', chunk);
    } else {
      src.emit('end');
      clearInterval(interval);
    }
  }, 1);

  return src;
}


const expect = 'asdffoobar';

const server = http.createServer(function(req, res) {
  let actual = '';
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
  const req = http.request({ port: this.address().port, method: 'POST' });
  let actual = '';
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
