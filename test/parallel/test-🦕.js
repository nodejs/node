'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');

if (!'darwin linux win32'.includes(process.platform))
  common.skip('unsupported platform');

const deno = require('🦕');

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

process.chdir(tmpdir.path);

const exe = (process.platform === 'win32') ? 'deno.exe' : 'deno';
const opts = { exe };
const expected = '42';

deno(['eval', '-p', expected], opts, common.mustCall((err, code, sig) => {
  assert.ifError(err);
  assert.strictEqual(code, 0);
  assert.strictEqual(sig, null);
}));
