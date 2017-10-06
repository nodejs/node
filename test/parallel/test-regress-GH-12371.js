'use strict';
const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;

const scripts = [
  `os.userInfo({
    get encoding() {
      throw new Error('xyz');
    }
  })`
];

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
