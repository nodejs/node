// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('node:assert');
const path = require('node:path');
const { extname } = require('node:internal/modules/esm/get_format');
const { fileURLToPath } = require('node:url');

[
  'file:///c:/path/to/file',
  'file:///c:/path/to/file.ext',
  'file:///c:/path.to/file.ext',
  'file:///c:/path.to/file',
  'file:///c:/path.to/.file',
  'file:///c:/path.to/.file.ext',
  'file:///c:/path/to/f.ext',
  'file:///c:/path/to/..ext',
  'file:///c:/path/to/..',
  'file:///c:/file',
  'file:///c:/file.ext',
  'file:///c:/.file',
  'file:///c:/.file.ext',
].forEach((input) => {
  const inputAsURL = new URL(input);
  const inputAsPath = fileURLToPath(inputAsURL);
  assert.strictEqual(extname(inputAsURL), path.extname(inputAsPath));
});
