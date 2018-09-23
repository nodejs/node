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

var gotError = false;
c.on('error', function(e) {
  console.error('couldn\'t connect.');
  gotError = true;
  assert.equal('ECONNREFUSED', e.code);
});


process.on('exit', function() {
  assert.ok(gotError);
});
