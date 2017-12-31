'use strict';
const common = require('../common');
const dgram = require('dgram');

// should not hang, see https://github.com/nodejs/node-v0.x-archive/issues/1282
dgram.createSocket('udp4');
dgram.createSocket('udp6');

{
  // Test the case of ref()'ing a socket with no handle.
  const s = dgram.createSocket('udp4');

  s.close(common.mustCall(() => s.ref()));
}
