'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { readFileSync } = require('node:fs');
const { test } = require('node:test');

function runWithReporter(reporter) {
  const destination = tmpdir.resolve(`${reporter}.out`);
  const args = [
    '--test-force-exit',
    `--test-reporter=${reporter}`,
    `--test-reporter-destination=${destination}`,
    fixtures.path('test-runner', 'reporters.js'),
  ];
  const child = spawnSync(process.execPath, args);
  assert.strictEqual(child.stdout.toString(), '');
  assert.strictEqual(child.stderr.toString(), '');
  assert.strictEqual(child.status, 1);
  return destination;
}

tmpdir.refresh();

test('junit reporter', () => {
  const output = readFileSync(runWithReporter('junit'), 'utf8');
  assert.match(output, /<!-- tests 4 -->/);
  assert.match(output, /<!-- pass 2 -->/);
  assert.match(output, /<!-- fail 2 -->/);
  assert.match(output, /<!-- duration_ms/);
  assert.match(output, /<\/testsuites>/);
});

test('spec reporter', () => {
  const output = readFileSync(runWithReporter('spec'), 'utf8');
  assert.match(output, /tests 4/);
  assert.match(output, /pass 2/);
  assert.match(output, /fail 2/);
});

test('tap reporter', () => {
  const output = readFileSync(runWithReporter('tap'), 'utf8');
  assert.match(output, /# tests 4/);
  assert.match(output, /# pass 2/);
  assert.match(output, /# fail 2/);
  assert.match(output, /# duration_ms/);
});
