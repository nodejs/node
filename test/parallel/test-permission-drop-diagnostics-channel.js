// Flags: --permission --allow-fs-read=* --allow-child-process
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const dc = require('node:diagnostics_channel');

const fsMessages = [];
dc.subscribe('node:permission-model:fs', (msg) => {
  fsMessages.push(msg);
});

const childMessages = [];
dc.subscribe('node:permission-model:child', (msg) => {
  childMessages.push(msg);
});

process.permission.drop('fs.read', '/tmp/test-drop');

assert.ok(fsMessages.length > 0, 'Expected at least one fs drop message');
assert.strictEqual(fsMessages[0].permission, 'FileSystemRead');
assert.strictEqual(fsMessages[0].drop, true);

process.permission.drop('child');

assert.ok(childMessages.length > 0, 'Expected at least one child drop message');
assert.strictEqual(childMessages[0].permission, 'ChildProcess');
assert.strictEqual(childMessages[0].drop, true);
