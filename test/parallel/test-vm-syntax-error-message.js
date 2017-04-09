'use strict';
require('../common');
const assert = require('assert');
const child_process = require('child_process');

const p = child_process.spawn(process.execPath, [
  '-e',
  'vm = require("vm");' +
      'context = vm.createContext({});' +
      'try { vm.runInContext("throw new Error(\'boo\')", context); } ' +
      'catch (e) { console.log(e.message); }'
]);

p.stderr.on('data', function(data) {
  assert.fail(`Unexpected stderr data: ${data}`);
});

let output = '';

p.stdout.on('data', function(data) {
  output += data;
});

process.on('exit', function() {
  assert.strictEqual(output.replace(/[\r\n]+/g, ''), 'boo');
});
