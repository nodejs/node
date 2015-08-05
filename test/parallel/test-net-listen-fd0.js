'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

var gotError = false;

process.on('exit', function() {
  assert(gotError instanceof Error);
});

// this should fail with an async EINVAL error, not throw an exception
net.createServer(assert.fail).listen({fd:0}).on('error', function(e) {
  switch(e.code) {
    case 'EINVAL':
    case 'ENOTSOCK':
      gotError = e;
      break;
  }
});
