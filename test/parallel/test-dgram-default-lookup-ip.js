'use strict';

// The default dgram lookup resolves a literal IP address of the socket's own
// family to itself, without calling dns.lookup(). Each case below stubs the
// process-global dns.lookup(), so they run sequentially to keep one case from
// observing another's stub.

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');

const originalLookup = dns.lookup;

function ipv4SendSkipsLookup(next) {
  dns.lookup = common.mustNotCall('dns.lookup() ran for an IPv4 literal');

  const receiver = dgram.createSocket('udp4');
  const sender = dgram.createSocket('udp4');

  receiver.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.toString(), 'payload');
    dns.lookup = originalLookup;
    receiver.close();
    sender.close();
    next();
  }));

  receiver.bind(0, '127.0.0.1', common.mustCall(() => {
    sender.send('payload', receiver.address().port, '127.0.0.1', common.mustCall());
  }));
}

function ipv6BindSkipsLookup(next) {
  if (!common.hasIPv6) {
    next();
    return;
  }

  dns.lookup = common.mustNotCall('dns.lookup() ran for an IPv6 literal');

  const socket = dgram.createSocket('udp6');

  socket.bind(0, '::1', common.mustCall(() => {
    dns.lookup = originalLookup;
    socket.close();
    next();
  }));
}

function mismatchedFamilyFallsThrough(next) {
  // '::1' is not an IPv4 literal, so a udp4 socket still resolves it via
  // dns.lookup() rather than short-circuiting.
  dns.lookup = common.mustCall((host, family, callback) => {
    dns.lookup = originalLookup;
    assert.strictEqual(host, '::1');
    assert.strictEqual(family, 4);
    callback(null, '127.0.0.1', 4);
  });

  const socket = dgram.createSocket('udp4');

  socket.bind(0, '::1', common.mustCall(() => {
    socket.close();
    next();
  }));
}

const cases = [
  ipv4SendSkipsLookup,
  ipv6BindSkipsLookup,
  mismatchedFamilyFallsThrough,
];

(function runNext() {
  const testCase = cases.shift();
  if (testCase !== undefined) {
    testCase(runNext);
  }
})();
