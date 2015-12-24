'use strict';
require('../common');
var assert = require('assert');
var child_process = require('child_process');

var p = child_process.spawn(process.execPath, [
  '-e',
  'vm = require("vm");' +
      'context = vm.createContext({});' +
      'try { vm.runInContext("throw new Error(\'boo\')", context); } ' +
      'catch (e) { console.log(e.message); }'
]);

p.stderr.on('data', function(data) {
  assert(false, 'Unexpected stderr data: ' + data);
});

var output = '';

p.stdout.on('data', function(data) {
  output += data;
});

process.on('exit', function() {
  assert.equal(output.replace(/[\r\n]+/g, ''), 'boo');
});
