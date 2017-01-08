'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  const code = 'let j = 0;\n' +
               'for (let i = 0; i < 1000000; i++) j += add(i, i + 1);\n' +
               'j;';

  const ctx = vm.createContext({
    add: function(x, y) {
      return x + y;
    }
  });

  vm.runInContext(code, ctx, { timeout: 1 });
} else {
  const proc = spawn(process.execPath, process.argv.slice(1).concat('child'));
  let err = '';
  proc.stderr.on('data', function(data) {
    err += data;
  });

  process.on('exit', function() {
    assert.ok(/Script execution timed out/.test(err));
  });
}
