'use strict';
const common = require('.');
const path = require('node:path');
const fs = require('node:fs/promises');
const assert = require('node:assert/strict');


const stackFramesRegexp = /(\s+)((.+?)\s+\()?(?:\(?(.+?):(\d+)(?::(\d+))?)\)?(\s+\{)?(\n|$)/g;
const windowNewlineRegexp = /\r/g;

function replaceStackTrace(str, replacement = '$1*$7\n') {
  return str.replace(stackFramesRegexp, replacement);
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
  const flags = common.parseTestFlags(filename);
  const { stdout, stderr } = await common.spawnPromisified(process.execPath, [...flags, filename]);
  await assertSnapshot(transform(`${stdout}${stderr}`), filename);
}

module.exports = {
  assertSnapshot,
  getSnapshotPath,
  replaceStackTrace,
  replaceWindowsLineEndings,
  spawnAndAssert,
  transform,
};
