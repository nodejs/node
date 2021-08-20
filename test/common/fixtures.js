'use strict';

const path = require('path');
const fs = require('fs');

const fixturesDir = path.join(__dirname, '..', 'fixtures');

function fixturesPath(...args) {
  return path.join(fixturesDir, ...args);
}

function readFixtureSync(args, enc) {
  if (Array.isArray(args))
    return fs.readFileSync(fixturesPath(...args), enc);
  return fs.readFileSync(fixturesPath(args), enc);
}

function readFixtureKey(name, enc) {
  return fs.readFileSync(fixturesPath('keys', name), enc);
}

function readFixtureKeys(enc, ...names) {
  return names.map((name) => readFixtureKey(name, enc));
}

module.exports = {
  fixturesDir,
  path: fixturesPath,
  readSync: readFixtureSync,
  readKey: readFixtureKey,
  readKeys: readFixtureKeys,
};
