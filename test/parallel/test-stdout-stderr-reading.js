'use strict';
const common = require('../common');
var assert = require('assert');

// verify that stdout is never read from.
var net = require('net');
var read = net.Socket.prototype.read;

net.Socket.prototype.read = function() {
  if (this.fd === 1)
    throw new Error('reading from stdout!');
  if (this.fd === 2)
    throw new Error('reading from stderr!');
  return read.apply(this, arguments);
};

if (process.argv[2] === 'child')
  child();
else
  parent();

function parent() {
  var spawn = require('child_process').spawn;
  var node = process.execPath;

  var c1 = spawn(node, [__filename, 'child']);
  var c1out = '';
  c1.stdout.setEncoding('utf8');
  c1.stdout.on('data', function(chunk) {
    c1out += chunk;
  });
  c1.stderr.setEncoding('utf8');
  c1.stderr.on('data', function(chunk) {
    console.error('c1err: ' + chunk.split('\n').join('\nc1err: '));
  });
  c1.on('close', common.mustCall(function(code, signal) {
    assert(!code);
    assert(!signal);
    assert.equal(c1out, 'ok\n');
    console.log('ok');
  }));

  var c2 = spawn(node, ['-e', 'console.log("ok")']);
  var c2out = '';
  c2.stdout.setEncoding('utf8');
  c2.stdout.on('data', function(chunk) {
    c2out += chunk;
  });
  c1.stderr.setEncoding('utf8');
  c1.stderr.on('data', function(chunk) {
    console.error('c1err: ' + chunk.split('\n').join('\nc1err: '));
  });
  c2.on('close', common.mustCall(function(code, signal) {
    assert(!code);
    assert(!signal);
    assert.equal(c2out, 'ok\n');
    console.log('ok');
  }));
}

function child() {
  // should not be reading *ever* in here.
  net.Socket.prototype.read = function() {
    throw new Error('no reading allowed in child');
  };
  console.log('ok');
}
