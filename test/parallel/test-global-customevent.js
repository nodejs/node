// Flags: --experimental-global-customevent --expose-internals
'use strict';

const common = require('../common');
const { strictEqual, ok } = require('node:assert');
const { Worker, isMainThread } = require('worker_threads');
const { CustomEvent } = require('internal/event_target');

ok(CustomEvent);
strictEqual(globalThis.CustomEvent, CustomEvent);

if (isMainThread) {
  const w = new Worker(__filename);
  w.on('message', common.mustNotCall());
  w.on('error', common.mustNotCall());
}
