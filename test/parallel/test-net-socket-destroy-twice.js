'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var nerrors = 0;
var ncloses = 0;

process.on('exit', function() {
  assert.equal(nerrors, 1);
  assert.equal(ncloses, 1);
});

var conn = net.createConnection(common.PORT);

conn.on('error', function() {
  nerrors++;
  conn.destroy();
});

conn.on('close', function() {
  ncloses++;
});
