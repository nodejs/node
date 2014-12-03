var common = require('../common');
var net = require('net');
var assert = require('assert');
var fp = '/tmp/fadagagsdfgsdf';
var c = net.connect(fp);

c.on('connect', assert.fail);

c.on('error', common.mustCall(function(e) {
  assert.equal(e.code, 'ENOENT');
  assert.equal(e.message, 'connect ENOENT ' + fp);
}));
