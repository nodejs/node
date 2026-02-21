'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');

// Test that invoking node with require, and piping stderr to file,
// does not result in exception,
// see: https://github.com/nodejs/node/issues/11257

tmpdir.refresh();
const fakeModulePath = tmpdir.resolve('batman.js');
const stderrOutputPath = tmpdir.resolve('stderr-output.txt');
// We need to redirect stderr to a file to produce #11257
const stream = fs.createWriteStream(stderrOutputPath);

// The error described in #11257 only happens when we require a
// non-built-in module.
fs.writeFileSync(fakeModulePath, '', 'utf8');

stream.on('open', () => {
  spawnSync(process.execPath, {
    input: `require(${JSON.stringify(fakeModulePath)})`,
    stdio: ['pipe', 'pipe', stream]
  });
  const stderr = fs.readFileSync(stderrOutputPath, 'utf8').trim();
  assert.strictEqual(
    stderr,
    '',
    `piping stderr to file should not result in exception: ${stderr}`
  );
  stream.end();
  fs.unlinkSync(stderrOutputPath);
  fs.unlinkSync(fakeModulePath);
});
