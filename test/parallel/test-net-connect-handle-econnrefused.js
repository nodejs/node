'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');


// Hopefully nothing is running on common.PORT
const c = net.createConnection(common.PORT);

c.on('connect', function() {
  console.error('connected?!');
  assert.ok(false);
});

c.on('error', common.mustCall(function(e) {
  console.error('couldn\'t connect.');
  assert.equal('ECONNREFUSED', e.code);
}));
