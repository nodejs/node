'use strict';
const common = require('../common');

if (common.isWindows || common.isAIX || common.isIBMi)
  common.skip(`No /dev/stdin on ${process.platform}.`);

const assert = require('assert');
const { exec } = require('child_process');

const tsCode = `
const message = "JavaScript from pipe";
console.log(message);
`;


const [cmd, opts] = common.escapePOSIXShell`printf "${tsCode}" | ${process.execPath} /dev/stdin`;

exec(cmd, opts, common.mustSucceed((stdout, stderr) => {
  assert.strictEqual(stdout.trim(), 'JavaScript from pipe');
  assert.strictEqual(stderr, '');
}));
