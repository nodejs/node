// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('node:assert');
const path = require('node:path');
const { extname } = require('node:internal/modules/esm/get_format');
const { fileURLToPath } = require('node:url');

[
  'file:///path/to/file',
  'file:///path/to/file.ext',
  'file:///path.to/file.ext',
  'file:///path.to/file',
  'file:///path.to/.file',
  'file:///path.to/.file.ext',
  'file:///path/to/f.ext',
  'file:///path/to/..ext',
  'file:///path/to/..',
  'file:///file',
  'file:///file.ext',
  'file:///.file',
  'file:///.file.ext',
].forEach((input) => {
  const inputAsURL = new URL(input);
  const inputAsPath = fileURLToPath(inputAsURL);
  assert.strictEqual(extname(inputAsURL), path.extname(inputAsPath));
});
