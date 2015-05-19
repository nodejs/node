'use strict';
var assert = require('assert');
var net = require('net');
var util = require('util');
var common = require('../common');
var revivals = 0;
var deaths = 0;

process.on('beforeExit', function() { deaths++; } );

process.once('beforeExit', tryImmediate);

function tryImmediate() {
  console.log('set immediate');
  setImmediate(function() {
    revivals++;
    process.once('beforeExit', tryTimer);
  });
}

function tryTimer() {
  console.log('set a timeout');
  setTimeout(function() {
    console.log('timeout cb, do another once beforeExit');
    revivals++;
    process.once('beforeExit', tryListen);
  }, 1);
}

function tryListen() {
  console.log('create a server');
  net.createServer()
    .listen(common.PORT)
    .on('listening', function() {
      revivals++;
      this.close();
    });
}

process.on('exit', function() {
  assert.equal(4, deaths);
  assert.equal(3, revivals);
});
