'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');
const fp = '/tmp/fadagagsdfgsdf';
const c = net.connect(fp);

c.on('connect', common.mustNotCall());

c.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.code, 'ENOENT');
  assert.strictEqual(e.message, 'connect ENOENT ' + fp);
}));
