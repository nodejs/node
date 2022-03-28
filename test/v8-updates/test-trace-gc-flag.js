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

  const scavengeRegex = /\bScavenge\b/;
  const expectedOutput = [
    scavengeRegex,
    scavengeRegex,
    scavengeRegex,
    scavengeRegex,
    /\bMark-sweep\b/,
  ];
  lines.forEach((line, index) => {
    assert.match(line, expectedOutput[index]);
  });
}

/**
 * HELPERS
 */

function splitByLine(str) {
  return str.split(/\n/);
}
