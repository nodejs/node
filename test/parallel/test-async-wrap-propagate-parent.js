'use strict';

require('../common');
const assert = require('assert');
const net = require('net');
const async_wrap = process.binding('async_wrap');
const providers = Object.keys(async_wrap.Providers);

const uidSymbol = Symbol('uid');

let cntr = 0;
let client;

function init(uid, type, parentUid, parentHandle) {
  this[uidSymbol] = uid;

  if (parentHandle) {
    cntr++;
    // Cannot assert in init callback or will abort.
    process.nextTick(() => {
      assert.strictEqual(providers[type], 'TCPWRAP');
      assert.strictEqual(parentUid, server._handle[uidSymbol],
                         'server uid doesn\'t match parent uid');
      assert.strictEqual(parentHandle, server._handle,
                         'server handle doesn\'t match parent handle');
      assert.strictEqual(this, client._handle, 'client doesn\'t match context');
    });
  }
}

function noop() { }

async_wrap.setupHooks({ init });
async_wrap.enable();

const server = net.createServer(function(c) {
  client = c;
  // Allow init callback to run before closing.
  setImmediate(() => {
    c.end();
    this.close();
  });
}).listen(0, function() {
  net.connect(this.address().port, noop);
});


process.on('exit', function() {
  // init should have only been called once with a parent.
  assert.strictEqual(cntr, 1);
});
