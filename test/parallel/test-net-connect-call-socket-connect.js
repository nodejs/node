'use strict';
const common = require('../common');

// This test checks that calling `net.connect` internally calls
// `Socket.prototype.connect`.
//
// This is important for people who monkey-patch `Socket.prototype.connect`
// since it's not possible to monkey-patch `net.connect` directly (as the core
// `connect` function is called internally in Node instead of calling the
// `exports.connect` function).
//
// Monkey-patching of `Socket.prototype.connect` is done by - among others -
// most APM vendors, the async-listener module and the
// continuation-local-storage module.
//
// Related:
// - https://github.com/nodejs/node/pull/12342
// - https://github.com/nodejs/node/pull/12852

const net = require('net');
const Socket = net.Socket;

// Monkey patch Socket.prototype.connect to check that it's called.
const orig = Socket.prototype.connect;
Socket.prototype.connect = common.mustCall(function() {
  return orig.apply(this, arguments);
});

const server = net.createServer();

server.listen(common.mustCall(function() {
  const port = server.address().port;
  const client = net.connect({port}, common.mustCall(function() {
    client.end();
  }));
  client.on('end', common.mustCall(function() {
    server.close();
  }));
}));
