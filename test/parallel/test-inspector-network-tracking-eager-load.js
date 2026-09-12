// Flags: --expose-internals --inspect=0 --experimental-network-inspection
'use strict';

// Regression test for https://github.com/nodejs/node/issues/64308.
//
// A `Network.enable` inspector message is dispatched to the main thread via an
// interrupt that can land in the middle of arbitrary JavaScript execution,
// including while another module is still being loaded by require(). If the
// network tracking enable() path lazily require()s its modules (which pull in
// `inspector`, `worker_threads` and `stream`) at that point, it can observe a
// half-initialized module and throw. The inspector agent treats any exception
// thrown while toggling network tracking as an unrecoverable fatal error and
// aborts the whole process.
//
// Guard against a regression by asserting that the network tracking modules
// are loaded eagerly at setup time and that enable()/disable() do not load any
// module themselves.

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { enable, disable } = require('internal/inspector_network_tracking');

// With --experimental-network-inspection, the tracking modules must already be
// loaded (they are required at module scope of internal/inspector_network_tracking,
// which itself is loaded during bootstrap when the flag is set).
for (const mod of [
  'NativeModule internal/inspector/network_http',
  'NativeModule internal/inspector/network_http2',
  'NativeModule internal/inspector/network_undici',
]) {
  assert(process.moduleLoadList.includes(mod),
         `${mod} should be loaded eagerly at setup, but was not`);
}

// enable()/disable() must not trigger any further module loading, so that they
// are safe to run from an inspector interrupt that fires mid-require().
const before = process.moduleLoadList.length;
enable();
disable();
assert.strictEqual(process.moduleLoadList.length, before,
                   'enable()/disable() must not lazily load any module');
