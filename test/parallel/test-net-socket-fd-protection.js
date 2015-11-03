'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

var will_throw = function() {
  var errors = [];
  [ 3, 4, 5, 6, 7, 8, 9 ].forEach(function(fd) {
    try {
      var stream = new net.Socket({
        fd: fd,
        readable: false,
        writable: true
      });
      stream.on('error', function() {});
      stream.write('might crash');
    } catch (e) {
      errors.push(e);
    }
  });
  throw errors;
};

// Should throw instead of crash
assert.throws(will_throw, /fd is being protected/);
