'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var gotError = 0;

process.on('exit', function() {
  assert.equal(gotError, 2);
});

net.createServer(common.fail).listen({fd:2}).on('error', onError);
net.createServer(common.fail).listen({fd:42}).on('error', onError);

function onError(ex) {
  assert.equal(ex.code, 'EINVAL');
  gotError++;
}
