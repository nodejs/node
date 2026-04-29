// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { defaultGetFormat } = require('internal/modules/esm/get_format');

// Regression test for #61904. This malformed data URL path is crafted to
// stress the MIME regex without triggering URL parsing failures.
const longPath = `data:a/${'a'.repeat(2 ** 17)}B`;
const start = process.hrtime.bigint();
const format = defaultGetFormat(new URL(longPath), { parentURL: undefined });
const elapsedMs = Number(process.hrtime.bigint() - start) / 1e6;

assert.strictEqual(format, null);
assert.ok(
  elapsedMs < common.platformTimeout(1000),
  `Expected format detection to complete quickly, took ${elapsedMs}ms`,
);
