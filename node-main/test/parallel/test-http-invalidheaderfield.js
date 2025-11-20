'use strict';
const common = require('../common');

const assert = require('assert');
const EventEmitter = require('events');
const http = require('http');

const ee = new EventEmitter();
let count = 3;

const server = http.createServer(function(req, res) {
  res.setHeader('testing_123', 123);
  assert.throws(function() {
    res.setHeader('testing 123', 123);
  }, TypeError);
  res.end('');
});
server.listen(0, function() {

  http.get({ port: this.address().port }, function() {
    ee.emit('done');
  });

  assert.throws(
    function() {
      const options = {
        port: server.address().port,
        headers: { 'testing 123': 123 }
      };
      http.get(options, common.mustNotCall());
    },
    function(err) {
      ee.emit('done');
      if (err instanceof TypeError) return true;
    }
  );

  // Should not throw.
  const options = {
    port: server.address().port,
    headers: { 'testing_123': 123 }
  };
  http.get(options, function() {
    ee.emit('done');
  });
});

ee.on('done', function() {
  if (--count === 0) {
    server.close();
  }
});
