'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const join = require('path').join;
const spawn = require('child_process').spawnSync;

// Test that invoking node with require, and piping stderr to file,
// does not result in exception,
// see: https://github.com/nodejs/node/issues/11257

tmpdir.refresh();
const fakeModulePath = join(tmpdir.path, 'batman.js');
const stderrOutputPath = join(tmpdir.path, 'stderr-output.txt');
// We need to redirect stderr to a file to produce #11257
const stream = fs.createWriteStream(stderrOutputPath);

// The error described in #11257 only happens when we require a
// non-built-in module.
fs.writeFileSync(fakeModulePath, '', 'utf8');

stream.on('open', () => {
  spawn(process.execPath, {
    input: `require("${fakeModulePath.replace(/\\/g, '/')}")`,
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
