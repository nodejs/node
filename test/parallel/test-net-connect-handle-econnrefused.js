'use strict';
var common = require('../common');
var net = require('net');
var assert = require('assert');


// Hopefully nothing is running on common.PORT
var c = net.createConnection(common.PORT);

c.on('connect', function() {
  console.error('connected?!');
  assert.ok(false);
});

c.on('error', common.mustCall(function(e) {
  console.error('couldn\'t connect.');
  assert.equal('ECONNREFUSED', e.code);
}));
