'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const REPORT_SECTIONS = [ 'header',
                          'javascriptStack',
                          'nativeStack',
                          'javascriptHeap',
                          'libuv',
                          'environmentVariables',
                          'sharedObjects' ];

let tmppath = '';

exports.findReports = (pid, path) => {
  // Default filenames are of the form
  // report.<date>.<time>.<pid>.<seq>.json
  tmppath = path;
  const format = '^report\\.\\d+\\.\\d+\\.' + pid + '\\.\\d+\\.json$';
  const filePattern = new RegExp(format);
  const files = fs.readdirSync(path);
  return files.filter((file) => filePattern.test(file));
};

exports.validate = (report, options) => {
  const jtmp = path.join(tmppath, report);
  fs.readFile(jtmp, (err, data) => {
    this.validateContent(data, options);
  });
};


exports.validateContent = function validateContent(data, options) {
  const report = JSON.parse(data);
  const comp = Object.keys(report);

  // Check all sections are present
  REPORT_SECTIONS.forEach((section) => {
    assert.ok(comp.includes(section));
  });
};
