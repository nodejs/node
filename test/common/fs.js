'use strict';

const { mustNotMutateObjectDeep } = require('.');
const { readdirSync } = require('node:fs');
const { join } = require('node:path');
const assert = require('node:assert');
const tmpdir = require('./tmpdir.js');

let dirc = 0;
function nextdir(dirname) {
  return tmpdir.resolve(dirname || `copy_%${++dirc}`);
}

function assertDirEquivalent(dir1, dir2) {
  const dir1Entries = [];
  collectEntries(dir1, dir1Entries);
  const dir2Entries = [];
  collectEntries(dir2, dir2Entries);
  assert.strictEqual(dir1Entries.length, dir2Entries.length);
  for (const entry1 of dir1Entries) {
    const entry2 = dir2Entries.find((entry) => {
      return entry.name === entry1.name;
    });
    assert(entry2, `entry ${entry2.name} not copied`);
    if (entry1.isFile()) {
      assert(entry2.isFile(), `${entry2.name} was not file`);
    } else if (entry1.isDirectory()) {
      assert(entry2.isDirectory(), `${entry2.name} was not directory`);
    } else if (entry1.isSymbolicLink()) {
      assert(entry2.isSymbolicLink(), `${entry2.name} was not symlink`);
    }
  }
}

function collectEntries(dir, dirEntries) {
  const newEntries = readdirSync(dir, mustNotMutateObjectDeep({ withFileTypes: true }));
  for (const entry of newEntries) {
    if (entry.isDirectory()) {
      collectEntries(join(dir, entry.name), dirEntries);
    }
  }
  dirEntries.push(...newEntries);
}

module.exports = {
  nextdir,
  assertDirEquivalent,
  collectEntries,
};
