'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const child = cp.spawn(process.execPath, ['--interactive']);
const fixture = path.join(common.fixturesDir, 'is-object.js').replace(/\\/g,
                                                                      '/');
let output = '';
let cmds = [
  'const isObject = (obj) => obj.constructor === Object;',
  'isObject({});',
  `require('${fixture}').isObject({});`
];

const run = ([cmd, ...rest]) => {
  if (!cmd) {
    setTimeout(() => child.stdin.end(), 100);
    return;
  }
  cmds = rest;
  child.stdin.write(`${cmd}\n`);
};

child.stdout.setEncoding('utf8');
child.stdout.on('data', (data) => {
  if (data !== '> ') {
    setTimeout(() => run(cmds), 100);
  }
  output += data;
});

child.on('exit', common.mustCall(() => {
  const results = output.replace(/^> /mg, '').split('\n');
  assert.deepStrictEqual(results, ['undefined', 'true', 'true', '']);
}));

run(cmds);
