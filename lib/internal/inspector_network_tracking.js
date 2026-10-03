'use strict';

// The network tracking modules are loaded eagerly here, at setup time, rather
// than lazily inside enable()/disable(). enable() can be invoked from an
// inspector interrupt triggered by a `Network.enable` message, which lands in
// the middle of arbitrary JavaScript execution -- including while another
// module is still being loaded by require(). Loading these modules at that
// point (they pull in `inspector`, `worker_threads` and `stream`) can observe
// a half-initialized module and throw, which the inspector agent turns into a
// fatal, unrecoverable error that aborts the process.
// Refs: https://github.com/nodejs/node/issues/64308
const networkHttp = require('internal/inspector/network_http');
const networkHttp2 = require('internal/inspector/network_http2');
const networkUndici = require('internal/inspector/network_undici');

function enable() {
  networkHttp.enable();
  networkHttp2.enable();
  networkUndici.enable();
}

function disable() {
  networkHttp.disable();
  networkHttp2.disable();
  networkUndici.disable();
}

module.exports = {
  enable,
  disable,
};
