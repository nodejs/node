'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { describe, it } = require('node:test');
const { spawnSync } = require('node:child_process');
const assert = require('node:assert');

const testFile = fixtures.path('test-runner/bail/bail.js');
tmpdir.refresh();

describe('node:test bail', () => {
  it('should exit at first failure', async () => {
    const child = spawnSync(process.execPath, ['--test', '--test-bail', testFile]);
    assert.strictEqual(child.stderr.toString(), '');
    assert.match(child.stdout.toString(), /ok 1 - first/);
    assert.match(child.stdout.toString(), /not ok 2 - second/);
    assert.match(child.stdout.toString(), /ok 3 - third/);
    assert.match(child.stdout.toString(), /not ok 1 - nested/);
    assert.doesNotMatch(child.stdout.toString(), /ok 1 - ok forth/);
    assert.doesNotMatch(child.stdout.toString(), /not ok 2 - fifth/);
    assert.doesNotMatch(child.stdout.toString(), /Subtest: top level/);
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
