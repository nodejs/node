'use strict';

// This tests that fs.readFile{Sync} on closed stdin works.

const { spawn } = require('child_process');
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

// fs.readFileSync(0)
{
  const script = fixtures.path('readfile-stdin-sync.js');
  const child = spawn(process.execPath, [script], {
    stdio: ['ignore', 'pipe', 'pipe']
  });
  child.stdout.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk.toString(), '\n');
  }));
  child.stderr.on('data', (chunk) => {
    assert.fail(chunk.toString());
  });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}

// fs.readFile(0)
{
  const script = fixtures.path('readfile-stdin-async.js');
  const child = spawn(process.execPath, [script], {
    stdio: ['ignore', 'pipe', 'pipe']
  });
  child.stdout.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk.toString(), '\n');
  }));
  child.stderr.on('data', (chunk) => {
    assert.fail(chunk.toString());
  });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
