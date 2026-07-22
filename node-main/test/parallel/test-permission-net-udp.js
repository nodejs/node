// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const dgram = require('dgram');

{
  const socket = dgram.createSocket('udp4');

  socket.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  socket.bind(0, common.mustNotCall());
}
