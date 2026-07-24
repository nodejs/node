'use strict';
// Verifies that fs.createReadStream()/createWriteStream() accept a raw Win32
// HANDLE through the `windowsHandle` option, as happens when a parent process
// passes an inherited anonymous pipe handle. The addon produces such handles
// via CreatePipe(); Node must wrap them in CRT file descriptors instead of
// failing with EBADF.

const common = require('../../common');

if (!common.isWindows) {
  common.skip('windowsHandle is Windows-only');
}

const assert = require('assert');
const fs = require('fs');

const binding = require(`./build/${common.buildType}/binding`);

const { readHandle, writeHandle } = binding.createPipeHandles();
assert.strictEqual(typeof readHandle, 'bigint');
assert.strictEqual(typeof writeHandle, 'bigint');

const payload = 'payload';

const chunks = [];
const rs = fs.createReadStream(null, { windowsHandle: readHandle });
rs.on('error', (err) => assert.fail(err));
rs.on('data', (chunk) => chunks.push(chunk));
rs.on('end', common.mustCall(() => {
  assert.strictEqual(Buffer.concat(chunks).toString(), payload);
}));

const ws = fs.createWriteStream(null, { windowsHandle: writeHandle });
ws.on('error', (err) => assert.fail(err));
ws.end(payload);
