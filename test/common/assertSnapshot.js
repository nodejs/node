'use strict';
const common = require('.');
const path = require('node:path');
const fs = require('node:fs/promises');
const assert = require('node:assert/strict');


const stackFramesRegexp = /(\s+)((.+?)\s+\()?(?:\(?(.+?):(\d+)(?::(\d+))?)\)?(\s+\{)?(\n|$)/g;
const windowNewlineRegexp = /\r/g;

function replaceStackTrace(str) {
  return str.replace(stackFramesRegexp, '$1*$7\n');
}

function replaceWindowsLineEndings(str) {
  return str.replace(windowNewlineRegexp, '');
}

function transform(...args) {
  return (str) => args.reduce((acc, fn) => fn(acc), str);
}

function getSnapshotPath(filename) {
  const { name, dir } = path.parse(filename);
  return path.resolve(dir, `${name}.snapshot`);
}

async function assertSnapshot(actual, filename = process.argv[1]) {
  const snapshot = getSnapshotPath(filename);
  if (process.env.NODE_REGENERATE_SNAPSHOTS) {
    await fs.writeFile(snapshot, actual);
  } else {
    const expected = await fs.readFile(snapshot, 'utf8');
    assert.strictEqual(actual, replaceWindowsLineEndings(expected));
  }
}

async function spawnAndAssert(filename, transform = (x) => x) {
  // TODO: Add an option to this function to alternatively or additionally compare stderr.
  // For now, tests that want to check stderr or both stdout and stderr can use spawnPromisified.
  const flags = common.parseTestFlags(filename);
  const { stdout } = await common.spawnPromisified(process.execPath, [...flags, filename]);
  await assertSnapshot(transform(stdout), filename);
}

module.exports = {
  assertSnapshot,
  getSnapshotPath,
  replaceStackTrace,
  replaceWindowsLineEndings,
  spawnAndAssert,
  transform,
};
