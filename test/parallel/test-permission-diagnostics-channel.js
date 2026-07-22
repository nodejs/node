// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');

// JS-only channels must not consume the native subscriber storage used by the
// permission audit publisher.
for (let i = 0; i < 1024 * 10 + 1; i++) {
  dc.channel(`test:permission:unrelated:${i}`);
}

const messages = [];
dc.subscribe('node:permission-model:fs', (msg) => {
  messages.push(msg);
});

// Granted permission should not publish
fs.readFileSync(__filename);
assert.strictEqual(messages.length, 0);

// Denied permission should publish
const hasWrite = process.permission.has('fs.write', '/tmp/test');
assert.strictEqual(hasWrite, false);

assert.ok(messages.length > 0, 'Expected at least one denied message');
assert.strictEqual(messages[0].permission, 'FileSystemWrite');
