'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');

const originalLookup = dns.lookup;

{
  // Verify that the provided lookup function is called.
  const lookup = common.mustCall((host, family, callback) => {
    assert.strictEqual(host, 'example.invalid');
    callback(null, '127.0.0.1', 4);
  });

  const socket = dgram.createSocket({ type: 'udp4', lookup });

  socket.bind(0, 'example.invalid', common.mustCall(() => {
    socket.close();
  }));
}

{
  // IPs resolve to themselves, so a custom lookup is not called.
  const lookup = common.mustNotCall('lookup ran for a literal IP address');

  const socket = dgram.createSocket({ type: 'udp4', lookup });

  socket.bind(0, '127.0.0.1', common.mustCall(() => {
    socket.close();
  }));
}

{
  // Verify that lookup defaults to dns.lookup().
  dns.lookup = common.mustCall((host, family, callback) => {
    dns.lookup = originalLookup;
    assert.strictEqual(host, 'example.invalid');
    assert.strictEqual(family, 4);
    callback(null, '127.0.0.1', 4);
  });

  const socket = dgram.createSocket({ type: 'udp4' });

  socket.bind(0, 'example.invalid', common.mustCall(() => {
    socket.close();
  }));
}

{
  // Verify that non-functions throw.
  [null, true, false, 0, 1, NaN, '', 'foo', {}, Symbol()].forEach((value) => {
    assert.throws(() => {
      dgram.createSocket({ type: 'udp4', lookup: value });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "lookup" argument must be of type function.' +
               common.invalidArgTypeHelper(value)
    });
  });
}
