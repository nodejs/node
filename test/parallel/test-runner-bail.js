'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { describe, it } = require('node:test');
const { spawnSync } = require('node:child_process');
const assert = require('node:assert');

const testFile = fixtures.path('test-runner/bail/bail.js');
tmpdir.refresh();

describe('node:test bail tap', () => {
  it('should exit at first failure', async () => {
    const child = spawnSync(process.execPath, ['--test', '--test-bail', testFile]);
    assert.strictEqual(child.stderr.toString(), '');
    assert.match(child.stdout.toString(), /ok 1 - first/);
    assert.match(child.stdout.toString(), /not ok 2 - second/);
    assert.doesNotMatch(child.stdout.toString(), /ok 3 - third/);
    assert.match(child.stdout.toString(), /not ok 1 - nested/);
    assert.doesNotMatch(child.stdout.toString(), /ok 1 - ok forth/);
    assert.doesNotMatch(child.stdout.toString(), /not ok 2 - fifth/);
  });

  it('should exit not exit if bail isnt set', async () => {
    const child = spawnSync(process.execPath, ['--test', testFile]);
    assert.strictEqual(child.stderr.toString(), '');
    assert.match(child.stdout.toString(), /ok 1 - first/);
    assert.match(child.stdout.toString(), /not ok 2 - second/);
    assert.match(child.stdout.toString(), /not ok 3 - third/);
    assert.match(child.stdout.toString(), /not ok 1 - nested/);
    assert.match(child.stdout.toString(), /ok 1 - forth/);
    assert.match(child.stdout.toString(), /not ok 2 - fifth/);
    assert.match(child.stdout.toString(), /not ok 2 - top level/);
  });
});

describe('node:test bail spec', () => {
  it('should exit at first failure', async () => {
    const child = spawnSync(process.execPath, ['--test', '--test-bail', '--test-reporter=spec', testFile]);
    assert.strictEqual(child.stderr.toString(), '');
    assert.match(child.stdout.toString(), /✔ first/);
    assert.match(child.stdout.toString(), /✖ second/);
    assert.doesNotMatch(child.stdout.toString(), /✖ third/);
    assert.doesNotMatch(child.stdout.toString(), /✔ forth/);
    assert.doesNotMatch(child.stdout.toString(), /✖ fifth/);
  });

  it('should exit not exit if bail isnt set', async () => {
    const child = spawnSync(process.execPath, ['--test', '--test-reporter=spec', testFile]);
    assert.strictEqual(child.stderr.toString(), '');
    assert.match(child.stdout.toString(), /✔ first/);
    assert.match(child.stdout.toString(), /✖ second/);
    assert.match(child.stdout.toString(), /✖ third/);
    assert.match(child.stdout.toString(), /✔ forth/);
    assert.match(child.stdout.toString(), /✖ fifth/);
  });
});
