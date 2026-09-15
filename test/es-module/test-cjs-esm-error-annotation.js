'use strict';

// This test verifies that when a CommonJS module requires an ES module,
// the error annotation (arrow message) points to the user's require()
// call in their source file, not to an internal frame such as
// TracingChannel.traceSync in node:diagnostics_channel.
// Regression test for https://github.com/nodejs/node/issues/55350.

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const path = require('node:path');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');

const requiringEsm = path.resolve(
  fixtures.path('/es-modules/cjs-esm-esm.js')
);

describe('ERR_REQUIRE_ESM error annotation', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should point to the user require() call, not internal frames', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--no-experimental-require-module', requiringEsm,
    ]);

    assert.strictEqual(code, 1);

    const stderrStr = stderr.replaceAll('\r', '');

    // The error annotation should reference the user's file, not
    // node:diagnostics_channel or any other internal module.
    assert.ok(
      stderrStr.includes(requiringEsm),
      `Expected error output to reference ${requiringEsm}, got:\n${stderrStr}`
    );

    // The error annotation line should contain the path to the requiring
    // file. Do not assume it is the very first stderr line — internal
    // throw-location lines may precede the arrow annotation.
    const lines = stderrStr.split('\n');
    const fileAnnotationLine = lines.find((l) => l.includes(requiringEsm));
    assert.ok(
      fileAnnotationLine,
      `Expected an annotation line referencing the requiring file, got:\n` +
      lines.slice(0, 10).join('\n')
    );
    assert.ok(
      !fileAnnotationLine.includes('diagnostics_channel'),
      `Annotation line should not reference diagnostics_channel, got: "${fileAnnotationLine}"`
    );
  });
});
