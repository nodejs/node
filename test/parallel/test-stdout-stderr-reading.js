'use strict';
const common = require('../common');
const assert = require('assert');

// verify that stdout is never read from.
const net = require('net');
const read = net.Socket.prototype.read;

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
  const spawn = require('child_process').spawn;
  const node = process.execPath;

  const c1 = spawn(node, [__filename, 'child']);
  let c1out = '';
  c1.stdout.setEncoding('utf8');
  c1.stdout.on('data', function(chunk) {
    c1out += chunk;
  });
  c1.stderr.setEncoding('utf8');
  c1.stderr.on('data', function(chunk) {
    console.error(`c1err: ${chunk.split('\n').join('\nc1err: ')}`);
  });
  c1.on('close', common.mustCall(function(code, signal) {
    assert(!code);
    assert(!signal);
    assert.strictEqual(c1out, 'ok\n');
    console.log('ok');
  }));

  const c2 = spawn(node, ['-e', 'console.log("ok")']);
  let c2out = '';
  c2.stdout.setEncoding('utf8');
  c2.stdout.on('data', function(chunk) {
    c2out += chunk;
  });
  c1.stderr.setEncoding('utf8');
  c1.stderr.on('data', function(chunk) {
    console.error(`c1err: ${chunk.split('\n').join('\nc1err: ')}`);
  });
  c2.on('close', common.mustCall(function(code, signal) {
    assert(!code);
    assert(!signal);
    assert.strictEqual(c2out, 'ok\n');
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
