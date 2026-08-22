'use strict';

const assert = require('assert');
const { pathToFileURL } = require('url');

const allowedDir = process.env.ALLOWED_DIR;
const allowedFile = process.env.ALLOWED_FILE;
const deniedFile = process.env.DENIED_FILE;

// file: URL
{
  assert.strictEqual(process.permission.has('fs.read', pathToFileURL(allowedFile)), true);
  assert.strictEqual(process.permission.has('fs.read', pathToFileURL(deniedFile)), false);
  assert.throws(() => {
    process.permission.has('fs.read', new URL('https://example.com'));
  }, { code: 'ERR_INVALID_URL_SCHEME' });
}

// Non-fs scopes like net ignore the reference, so a non-file: URL must not be rejected here.
{
  assert.strictEqual(process.permission.has('net', new URL('https://example.com')), false);
}

// Uint8Array
{
  const allowedBytes = new Uint8Array(Buffer.from(allowedFile));
  const deniedBytes = new Uint8Array(Buffer.from(deniedFile));
  assert.strictEqual(process.permission.has('fs.read', allowedBytes), true);
  assert.strictEqual(process.permission.has('fs.read', deniedBytes), false);

  // Must respect byteOffset/byteLength, not read from the start of the buffer.
  const padded = Buffer.concat([Buffer.from('padding-'), Buffer.from(allowedFile)]);
  const view = new Uint8Array(padded.buffer, padded.byteOffset + 8, allowedFile.length);
  assert.strictEqual(process.permission.has('fs.read', view), true);
}

// drop() only matches the exact granted path (the directory), not a file inside it.
{
  process.permission.drop('fs.read', pathToFileURL(allowedDir));
  assert.strictEqual(process.permission.has('fs.read', pathToFileURL(allowedFile)), false);
}
