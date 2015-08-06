'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

var gotError = 0;

process.on('exit', function() {
  assert.equal(gotError, 2);
});

net.createServer(assert.fail).listen({fd:2}).on('error', onError);
net.createServer(assert.fail).listen({fd:42}).on('error', onError);

function onError(ex) {
  assert.equal(ex.code, 'EINVAL');
  gotError++;
}
