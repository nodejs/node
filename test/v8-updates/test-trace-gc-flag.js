'use strict';

// This test verifies that `--trace-gc` flag is well integrated.
// We'll check here, that the console outputs gc events properly.
require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');

const fixtures = require('../common/fixtures');

{
  const childProcess = spawnSync(process.execPath, [
    '--trace-gc',
    '--expose-gc',
    fixtures.path('gc.js'),
  ]);
  const output = childProcess.stdout.toString().trim();
  const lines = splitByLine(output);

  const isolateAddressRegex = /^my isolate address: (.*)$/;
  const isolateAddressLine = lines.find((line) => line.match(isolateAddressRegex));
  assert(isolateAddressLine);
  const isolateAddress = isolateAddressLine.match(isolateAddressRegex)[1];
  assert(isolateAddress);

  const traceGcLines = lines.filter((line) => line !== isolateAddressLine);

  const scavengeRegex = /\bScavenge\b/;
  const expectedOutput = [
    scavengeRegex,
    scavengeRegex,
    scavengeRegex,
    scavengeRegex,
    /\bMark-sweep\b/,
  ];

  assert.strictEqual(traceGcLines.length, expectedOutput.length);
  traceGcLines.forEach((line, index) => {
    assert.match(line, expectedOutput[index]);
  });

  const traceGcPrefix = `[${childProcess.pid}:${isolateAddress}] `;
  traceGcLines.forEach((line) => {
    assert(line.startsWith(traceGcPrefix));
  });
}

/**
 * HELPERS
 */

function splitByLine(str) {
  return str.split(/\n/);
}
