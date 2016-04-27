'use strict';
require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;
const execFileSync = require('child_process').execFileSync;

const code = 'setTimeout(() => {}, 400)';

spawn(process.argv[0], ['-e', code]);
spawn(process.argv[0], ['-e', code]);
assert(process.children().length === 2);

process.children().forEach((el) => {
  assert.doesNotThrow(() => {
    process.kill(el);
  }, Error);
});

const nested_child_code = `
  const spawn = require('child_process').spawn;
  spawn(process.argv[0], ['-e', 'setTimeout(()=>{})']);
  console.log(process.children().length === 1)
`;

const res = execFileSync(process.argv[0], ['-e', nested_child_code]);
assert(Buffer.compare(res, new Buffer('true\n')) === 0);

// For later regression: deregistering children is not implemented
assert(process.children().length !== 0);
