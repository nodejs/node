// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const { internalBinding } = require('internal/test/binding');
const { TCP, constants: TCPConstants } = internalBinding('tcp_wrap');
const { UV_EAFNOSUPPORT } = internalBinding('uv');

const probe = new TCP(TCPConstants.SOCKET);
if (probe.bind6('::1', 0, TCPConstants.UV_TCP_IPV6ONLY) !== 0) {
  probe.close();
  common.skip('no IPv6 support');
}
probe.close();

const bind6 = TCP.prototype.bind6;
let firstBind = true;

TCP.prototype.bind6 = function(...args) {
  if (firstBind && args[2] === 0) {
    firstBind = false;
    return UV_EAFNOSUPPORT;
  }
  return bind6.apply(this, args);
};

const server = net.createServer();
server.on('error', common.mustNotCall());

server.listen({ port: 0 }, common.mustCall(() => {
  assert.strictEqual(server.address().family, 'IPv6');

  server.close(common.mustCall(() => {
    TCP.prototype.bind6 = bind6;
  }));
}));
