// Flags: --inspect=0 --experimental-network-inspection
'use strict';

// Regression test for https://github.com/nodejs/node/issues/64308.
//
// A `Network.enable` inspector message is dispatched to the main thread via an
// interrupt that can land in the middle of arbitrary JavaScript execution,
// including while another module is still being loaded by require(). If
// enabling network tracking calls into JS at that point (the tracking modules
// pull in `inspector`, `worker_threads` and `stream`), it can observe a
// half-initialized module and throw, which the inspector agent treats as an
// unrecoverable fatal error and aborts the whole process.
//
// With --experimental-network-inspection, the tracking modules must therefore
// be loaded and their diagnostics channels subscribed at startup, so that
// `Network.enable`/`Network.disable` never call into JS from the interrupt.

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');

// The tracking modules must already be loaded: they are required and enabled
// during bootstrap when --experimental-network-inspection is set.
for (const mod of [
  'NativeModule internal/inspector_network_tracking',
  'NativeModule internal/inspector/network_http',
  'NativeModule internal/inspector/network_http2',
  'NativeModule internal/inspector/network_undici',
]) {
  assert(process.moduleLoadList.includes(mod),
         `${mod} should be loaded eagerly at startup, but was not`);
}

// The diagnostics channels must already be subscribed at startup, so toggling
// Network.enable does not need to run any JS.
const dc = require('diagnostics_channel');
for (const channel of [
  'http.client.request.created',
  'http2.client.stream.created',
  'undici:request:create',
]) {
  assert(dc.channel(channel).hasSubscribers,
         `${channel} should have a subscriber at startup, but does not`);
}
