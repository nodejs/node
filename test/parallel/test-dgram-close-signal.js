// Flags: --experimental-abortcontroller
'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

{
  // Test bad signal.
  assert.throws(
    () => dgram.createSocket({ type: 'udp4', signal: {} }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });
}

{
  // Test close.
  const controller = new AbortController();
  const { signal } = controller;
  const server = dgram.createSocket({ type: 'udp4', signal });
  server.on('close', common.mustCall());
  controller.abort();
}

{
  // Test close with pre-aborted signal.
  const controller = new AbortController();
  controller.abort();
  const { signal } = controller;
  const server = dgram.createSocket({ type: 'udp4', signal });
  server.on('close', common.mustCall());
}
