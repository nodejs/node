'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const async_wrap = process.binding('async_wrap');
const providers = Object.keys(async_wrap.Providers);

let cntr = 0;
let server;
let client;

function init(type, parent) {
  if (parent) {
    cntr++;
    // Cannot assert in init callback or will abort.
    process.nextTick(() => {
      assert.equal(providers[type], 'TCPWRAP');
      assert.equal(parent, server._handle, 'server doesn\'t match parent');
      assert.equal(this, client._handle, 'client doesn\'t match context');
    });
  }
}

function noop() { }

async_wrap.setupHooks(init, noop, noop);
async_wrap.enable();

server = net.createServer(function(c) {
  client = c;
  // Allow init callback to run before closing.
  setImmediate(() => {
    c.end();
    this.close();
  });
}).listen(common.PORT, function() {
  net.connect(common.PORT, noop);
});

async_wrap.disable();

process.on('exit', function() {
  // init should have only been called once with a parent.
  assert.equal(cntr, 1);
});
