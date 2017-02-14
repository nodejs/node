'use strict';
const common = require('../common');
const dgram = require('dgram');

{
  // Test the case of unref()'ing a socket with a handle.
  const s = dgram.createSocket('udp4');
  s.bind();
  s.unref();
}

{
  // Test the case of unref()'ing a socket with no handle.
  const s = dgram.createSocket('udp4');

  s.close(common.mustCall(() => s.unref()));
}

setTimeout(common.mustNotCall(), 1000).unref();
