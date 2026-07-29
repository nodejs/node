'use strict';

require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { getCallSites } = require('node:util');
const fixtures = require('../common/fixtures');

// mapCallSite falls back to the original call site when no source map is
// registered for the current file.
{
  const withoutMap = getCallSites(1);
  const withMap = getCallSites(1, { sourceMap: true });
  // scriptName must be identical — no source map registered for this test file
  assert.strictEqual(withMap[0].scriptName, withoutMap[0].scriptName);
  // Both calls report the actual line in this file (they differ by one because
  // they are on consecutive source lines, which is expected).
  assert.ok(typeof withMap[0].lineNumber === 'number');
  assert.ok(typeof withMap[0].columnNumber === 'number');
}

// reconstructCallSite remaps call sites to the original source when
// --enable-source-maps is active and a source map is found.
{
  const file = fixtures.path('source-map', 'get-call-sites-mapped.js');
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    ['--enable-source-maps', file],
  );
  assert.strictEqual(status, 0, stderr.toString());
  const callSite = JSON.parse(stdout.toString());
  // scriptName should point to the original (unmapped) source file
  assert.ok(
    callSite.scriptName.endsWith('get-call-sites-original.js'),
    `expected original source in scriptName, got "${callSite.scriptName}"`,
  );
  // lineNumber and columnNumber should reflect the original source position
  assert.strictEqual(callSite.lineNumber, 11);
  assert.strictEqual(callSite.columnNumber, 1);
}

// Without --enable-source-maps the generated file path is preserved.
{
  const file = fixtures.path('source-map', 'get-call-sites-mapped.js');
  const { status, stderr, stdout } = spawnSync(process.execPath, [file]);
  assert.strictEqual(status, 0, stderr.toString());
  const callSite = JSON.parse(stdout.toString());
  assert.ok(
    callSite.scriptName.endsWith('get-call-sites-mapped.js'),
    `expected generated file in scriptName, got "${callSite.scriptName}"`,
  );
}
