'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

const p = child_process.spawn(process.execPath, [
  '-e',
  'vm = require("vm");' +
      'context = vm.createContext({});' +
      'try { vm.runInContext("throw new Error(\'boo\')", context); } ' +
      'catch (e) { console.log(e.message); }'
]);

p.stderr.on('data', common.mustNotCall());

let output = '';

p.stdout.on('data', (data) => output += data);

p.stdout.on('end', common.mustCall(() => {
  assert.strictEqual(output.replace(/[\r\n]+/g, ''), 'boo');
}));
