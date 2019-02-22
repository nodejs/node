/* eslint-disable node-core/required-modules */
'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');

function findReports(pid, dir) {
  // Default filenames are of the form
  // report.<date>.<time>.<pid>.<seq>.json
  const format = '^report\\.\\d+\\.\\d+\\.' + pid + '\\.\\d+\\.json$';
  const filePattern = new RegExp(format);
  const files = fs.readdirSync(dir);
  const results = [];

  files.forEach((file) => {
    if (filePattern.test(file))
      results.push(path.join(dir, file));
  });

  return results;
}

function validate(report) {
  const data = fs.readFileSync(report, 'utf8');

  validateContent(data);
}

function validateContent(data) {
  const report = JSON.parse(data);

  // Verify that all sections are present.
  ['header', 'javascriptStack', 'nativeStack', 'javascriptHeap',
   'libuv', 'environmentVariables', 'sharedObjects'].forEach((section) => {
    assert(report.hasOwnProperty(section));
  });

  assert.deepStrictEqual(report.header.componentVersions, process.versions);
  assert.deepStrictEqual(report.header.release, process.release);
}

module.exports = { findReports, validate, validateContent };
