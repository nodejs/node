'use strict';
require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const http = require('http');

const ee = new EventEmitter();
var count = 3;

const server = http.createServer(function(req, res) {
  assert.doesNotThrow(function() {
    res.setHeader('testing_123', 123);
  });
  assert.throws(function() {
    res.setHeader('testing 123', 123);
  }, TypeError);
  res.end('');
});
server.listen(0, function() {

  http.get({port: this.address().port}, function() {
    ee.emit('done');
  });

  assert.throws(
    function() {
      var options = {
        port: server.address().port,
        headers: {'testing 123': 123}
      };
      http.get(options, function() {});
    },
    function(err) {
      ee.emit('done');
      if (err instanceof TypeError) return true;
    }
  );

  assert.doesNotThrow(
    function() {
      var options = {
        port: server.address().port,
        headers: {'testing_123': 123}
      };
      http.get(options, function() {
        ee.emit('done');
      });
    }, TypeError
  );
});

ee.on('done', function() {
  if (--count === 0) {
    server.close();
  }
});
