'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');
var fp = '/tmp/fadagagsdfgsdf';
var c = net.connect(fp);

c.on('connect', common.fail);

c.on('error', common.mustCall(function(e) {
  assert.equal(e.code, 'ENOENT');
  assert.equal(e.message, 'connect ENOENT ' + fp);
}));
