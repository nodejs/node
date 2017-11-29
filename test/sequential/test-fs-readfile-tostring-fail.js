'use strict';

const common = require('../common');

if (!common.enoughTestMem)
  common.skip('intensive toString tests due to memory confinements');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const cp = require('child_process');
const kStringMaxLength = process.binding('buffer').kStringMaxLength;
if (common.isAIX && (Number(cp.execSync('ulimit -f')) * 512) < kStringMaxLength)
  common.skip('intensive toString tests due to file size confinements');

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'toobig.txt');
const stream = fs.createWriteStream(file, {
  flags: 'a'
});

const size = kStringMaxLength / 200;
const a = Buffer.alloc(size, 'a');

for (let i = 0; i < 201; i++) {
  stream.write(a);
}

stream.end();
stream.on('finish', common.mustCall(function() {
  // make sure that the toString does not throw an error
  fs.readFile(file, 'utf8', common.mustCall(function(err, buf) {
    assert.ok(err instanceof Error);
    assert(/^(Array buffer allocation failed|"toString\(\)" failed)$/
             .test(err.message));
    assert.strictEqual(buf, undefined);
  }));
}));

function destroy() {
  try {
    fs.unlinkSync(file);
  } catch (err) {
    // it may not exist
  }
}

process.on('exit', destroy);

process.on('SIGINT', function() {
  destroy();
  process.exit();
});

// To make sure we don't leave a very large file
// on test machines in the event this test fails.
process.on('uncaughtException', function(err) {
  destroy();
  throw err;
});
