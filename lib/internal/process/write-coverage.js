'use strict';
const process = require('process');
const path = require('path');
const fs = require('fs');
const mkdirSync = fs.mkdirSync;
const writeFileSync = fs.writeFileSync;

function writeCoverage() {
  if (!global.__coverage__) {
    return;
  }

  const dirname = path.join(path.dirname(process.execPath), '.coverage');
  const filename = `coverage-${process.pid}-${Date.now()}.json`;
  try {
    mkdirSync(dirname);
  } catch (err) {
    if (err.code !== 'EEXIST') {
      console.error(err);
      return;
    }
  }

  const target = path.join(dirname, filename);
  const coverageInfo = JSON.stringify(global.__coverage__);
  try {
    writeFileSync(target, coverageInfo);
  } catch (err) {
    console.error(err);
  }
}

function setup() {
  const reallyReallyExit = process.reallyExit;

  process.reallyExit = function(code) {
    writeCoverage();
    reallyReallyExit(code);
  };

  process.on('exit', writeCoverage);
}

exports.setup = setup;
