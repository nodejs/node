'use strict';

// Network tracking is loaded and enabled at startup, when
// --experimental-network-inspection is set, rather than lazily from the
// inspector agent when a `Network.enable` message arrives. That message is
// dispatched via an interrupt that can land in the middle of arbitrary
// JavaScript execution -- including while another module is still being
// loaded by require(). Calling into JS at that point (the tracking modules
// pull in `inspector`, `worker_threads` and `stream`) can observe a
// half-initialized module and throw, which the inspector agent turns into a
// fatal, unrecoverable error that aborts the process.
//
// With the channels subscribed eagerly, `Network.enable`/`Network.disable`
// only toggle a flag in C++ (NetworkInspector::enabled_) that gates event
// emission, and never call into JS from the interrupt.
// Refs: https://github.com/nodejs/node/issues/64308
const networkHttp = require('internal/inspector/network_http');
const networkHttp2 = require('internal/inspector/network_http2');
const networkUndici = require('internal/inspector/network_undici');

function enable() {
  networkHttp.enable();
  networkHttp2.enable();
  networkUndici.enable();
}

module.exports = {
  enable,
};
