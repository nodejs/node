'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const UDP = process.binding('udp_wrap').UDP;
const _createSocketHandle = dgram._createSocketHandle;

// Throws if an "existing fd" is passed in.
common.expectsError(() => {
  _createSocketHandle(common.localhostIPv4, 0, 'udp4', 42);
}, {
  code: 'ERR_ASSERTION',
  message: /^false == true$/
});

{
  // Create a handle that is not bound.
  const handle = _createSocketHandle(null, null, 'udp4');

  assert(handle instanceof UDP);
  assert.strictEqual(typeof handle.fd, 'number');
  assert(handle.fd < 0);
}

{
  // Create a bound handle.
  const handle = _createSocketHandle(common.localhostIPv4, 0, 'udp4');

  assert(handle instanceof UDP);
  assert.strictEqual(typeof handle.fd, 'number');

  if (!common.isWindows)
    assert(handle.fd > 0);
}

{
  // Return an error if binding fails.
  const err = _createSocketHandle('localhost', 0, 'udp4');

  assert.strictEqual(typeof err, 'number');
  assert(err < 0);
}
