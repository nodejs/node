'use strict';
require('../common');
var assert = require('assert');
var vm = require('vm');
var spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  var code = 'var j = 0;\n' +
             'for (var i = 0; i < 1000000; i++) j += add(i, i + 1);\n' +
             'j;';

  var ctx = vm.createContext({
    add: function(x, y) {
      return x + y;
    }
  });

  vm.runInContext(code, ctx, { timeout: 1 });
} else {
  var proc = spawn(process.execPath, process.argv.slice(1).concat('child'));
  var err = '';
  proc.stderr.on('data', function(data) {
    err += data;
  });

  process.on('exit', function() {
    assert.ok(/Script execution timed out/.test(err));
  });
}
