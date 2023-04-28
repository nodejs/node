'use strict';
// Tests that vm.createScript and runInThisContext correctly handle errors
// thrown by option property getters.
// See https://github.com/nodejs/node/issues/12369.

const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;

const scripts = [];

['filename', 'cachedData', 'produceCachedData', 'lineOffset', 'columnOffset']
  .forEach((prop) => {
    scripts.push(`vm.createScript('', {
      get ${prop} () {
        throw new Error('xyz');
      }
    })`);
  });

['breakOnSigint', 'timeout', 'displayErrors']
  .forEach((prop) => {
    scripts.push(`vm.createScript('').runInThisContext({
      get ${prop} () {
        throw new Error('xyz');
      }
    })`);
  });

scripts.forEach((script) => {
  const node = process.execPath;
  execFile(node, [ '-e', script ], common.mustCall((err, stdout, stderr) => {
    assert(stderr.includes('Error: xyz'), 'createScript crashes');
  }));
});
